#include "soundout.h"

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QSysInfo>
#include <qmath.h>
#include <QDebug>

#include "moc_soundout.cpp"
#include "audio_device_policy.hpp"
#include "audio_startup_policy.hpp"

/* #if defined (WIN32)
# define MS_BUFFERED 1000u
#else
# define MS_BUFFERED 2000u
#endif */
# define MS_BUFFERED 200u

void SoundOutput::reportError (QString const& message)
{
  if (!m_errorReported)
    {
      m_errorReported = true;
      Q_EMIT error (message);
    }
}

bool SoundOutput::audioError ()
{
  bool result (true);
  Q_ASSERT_X (m_stream, "SoundOutput", "programming error");
  if (m_stream) {
    switch (m_stream->error ()) {
      case QAudio::OpenError: reportError (tr ("An error opening the audio output device has occurred.")); break;
      case QAudio::IOError: reportError (tr ("An error occurred during write to the audio output device.")); break;
      case QAudio::UnderrunError: reportError (tr ("Audio data not being fed to the audio output device fast enough.")); break;
      case QAudio::FatalError: reportError (tr ("Non-recoverable error, audio output device not usable at this time.")); break;
      case QAudio::NoError: result = false; break;
    }
  }
  return result;
}

void SoundOutput::setFormat (QAudioDeviceInfo const& device, unsigned channels, int frames_buffered)
{
  m_startupTimer.stop ();
  m_startupPending = false;
  Q_ASSERT (0 < channels && channels < 3);
  m_device = device;
  m_channels = channels;
  m_framesBuffered = frames_buffered;
  m_recreatePending = false;
  m_errorReported = false;
  QAudioFormat format (device.preferredFormat ());
//  qDebug () << "Preferred audio output format:" << format;
  format.setChannelCount (channels);
  format.setCodec ("audio/pcm");
  format.setSampleRate (48000);
  format.setSampleType (QAudioFormat::SignedInt);
  format.setSampleSize (16);
  format.setByteOrder (QAudioFormat::Endian (QSysInfo::ByteOrder));
  if(!format.isValid ()) {
    reportError (tr ("Requested output audio format is not valid."));
    m_stream.reset ();
    m_recreatePending = true;
    return;
  }
  if(!device.isFormatSupported (format)) {
    reportError (tr ("Requested output audio format is not supported on device."));
    m_stream.reset ();
    m_recreatePending = true;
    return;
  }
//  qDebug () << "Selected audio output format:" << format;
  m_stream.reset (new QAudioOutput (device, format));
  if (audioError ()) {
    m_recreatePending = true;
    return;
  }
  m_stream->setVolume (m_volume);
  m_stream->setNotifyInterval(100);
  connect (m_stream.data(), &QAudioOutput::stateChanged, this, &SoundOutput::handleStateChanged);
  connect (m_stream.data(), &QAudioOutput::notify, this, &SoundOutput::handleNotify);
  //      qDebug() << "A" << m_volume << m_stream->notifyInterval();
}

bool SoundOutput::recreateStream ()
{
  QAudioDeviceInfo device;
  bool exactObjectFound {false};
  unsigned sameNameCount {0};

  Q_FOREACH (auto const& candidate, QAudioDeviceInfo::availableDevices (QAudio::AudioOutput))
    {
      // Prefer the exact device object.  Name matching is evaluated in a
      // second pass so duplicate endpoint names can never be guessed.
      if (candidate == m_device)
        {
          device = candidate;
          exactObjectFound = true;
          break;
        }
    }

  if (!exactObjectFound)
    {
      Q_FOREACH (auto const& candidate, QAudioDeviceInfo::availableDevices (QAudio::AudioOutput))
        {
          if (candidate.deviceName () == m_device.deviceName ())
            {
              ++sameNameCount;
              device = candidate;
            }
        }
    }

  switch (AudioDeviceSelectionPolicy::choose (exactObjectFound, sameNameCount))
    {
    case AudioDeviceSelectionPolicy::Match::ExactObject:
    case AudioDeviceSelectionPolicy::Match::UniqueName:
      break;

    case AudioDeviceSelectionPolicy::Match::AmbiguousName:
      reportError (tr ("Multiple audio output devices share the configured name; refusing to guess: %1")
                   .arg (m_device.deviceName ()));
      return false;

    case AudioDeviceSelectionPolicy::Match::NotFound:
      reportError (tr ("The configured audio output device is not available: %1")
                   .arg (m_device.deviceName ()));
      return false;
    }

  setFormat (device, m_channels, m_framesBuffered);
  return m_stream && !m_recreatePending;
}

bool SoundOutput::restart (QIODevice * source)
{
  // Allow each requested transmission to report its own startup failure.
  m_errorReported = false;
  m_startupTimer.stop ();
  m_startupPending = false;
  m_progressReported = false;

#if defined (Q_OS_WIN)
  // Windows endpoint handles can remain apparently healthy after an idle USB
  // reset while silently producing no samples.  Re-enumerate and recreate the
  // output object before each real transmission start.
  if (!recreateStream ())
    {
      return false;
    }
#else
  if (m_recreatePending || !m_stream || QAudio::NoError != m_stream->error ())
    {
      if (!recreateStream ())
        {
          return false;
        }
    }
#endif

  if (!m_stream)
    {
      reportError (tr ("The audio output device could not be initialized."));
      m_recreatePending = true;
      return false;
    }

  // This buffer size is critical since for proper sound streaming. If
  // it is too short; high activity levels on the machine can starve
  // the audio buffer. On the other hand the Windows implementation
  // seems to take the length of the buffer in time to stop the audio
  // stream even if reset() is used.
  //
  // 2 seconds seems a reasonable compromise except for Windows
  // where things are probably broken.
  //
  // we have to set this before every start on the stream because the
  // Windows implementation seems to forget the buffer size after a
  // stop.
    if (m_framesBuffered > 0) { m_stream->setBufferSize (m_stream->format().bytesForFrames (m_framesBuffered)); }
  //  qDebug() << "B" << m_stream->bufferSize() <<
  //  m_stream->periodSize() << m_stream->notifyInterval();
  m_stream->setCategory ("production");
  m_startupPending = true;
  m_startupElapsed.start ();
  m_startupProcessedUSecs = m_stream->processedUSecs ();
  m_stream->start (source);
  if (audioError ())
    {
      m_recreatePending = true;
      m_startupPending = false;
      m_startupTimer.stop ();
      return false;
    }

  auto const decision = AudioStartupPolicy::decide (
    QAudio::ActiveState == m_stream->state (),
    QAudio::StoppedState == m_stream->state (),
#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
    QAudio::InterruptedState == m_stream->state (),
#else
    false,
#endif
    0, 500);
  if (AudioStartupPolicy::Decision::Confirm == decision)
    {
      confirmStartup ();
    }
  else if (AudioStartupPolicy::Decision::Fail == decision)
    {
      failStartup (tr ("The audio output device stopped before startup confirmation."));
      return false;
    }
  else
    {
      m_startupTimer.start (500);
    }
  return !m_recreatePending;
}

void SoundOutput::suspend ()
{ if (m_stream && QAudio::ActiveState == m_stream->state ()) { m_stream->suspend (); audioError (); } }

void SoundOutput::resume ()
{ if(m_stream && QAudio::SuspendedState == m_stream->state ()) { m_stream->resume (); audioError (); } }

void SoundOutput::reset ()
{ if (m_stream) { m_stream->reset (); audioError (); } }

void SoundOutput::stop ()
{ if(m_stream) { m_stream->stop (); audioError (); } }

qreal SoundOutput::attenuation () const
{ return -(20. * qLn (m_volume) / qLn (10.)); }

void SoundOutput::setAttenuation (qreal a)
{
  Q_ASSERT (0.0 <= a && a <= 450.1);
  m_volume = qPow(10.0, -a/20.0)*0.9; // 0.9 is the workaround to prevent audio stream distortion in VAC software
  //  qDebug () << "SoundOut: attn = " << a << ", vol = " << m_volume;
  if(m_stream) m_stream->setVolume (m_volume);
}

void SoundOutput::resetAttenuation ()
{ m_volume = 1.; if(m_stream) m_stream->setVolume (m_volume); }

void SoundOutput::handleStateChanged (QAudio::State newState)
{
  // qDebug () << "SoundOutput::handleStateChanged: newState:" << newState;
  if (m_startupPending)
    {
      auto const decision = AudioStartupPolicy::decide (
        QAudio::ActiveState == newState,
        QAudio::StoppedState == newState,
#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
        QAudio::InterruptedState == newState,
#else
        false,
#endif
        m_startupElapsed.isValid () ? m_startupElapsed.elapsed () : 0, 500);
      if (AudioStartupPolicy::Decision::Confirm == decision)
        {
          confirmStartup ();
        }
      else if (AudioStartupPolicy::Decision::Fail == decision)
        {
          if (QAudio::StoppedState == newState && audioError ())
            {
              m_recreatePending = true;
              m_startupPending = false;
              m_startupTimer.stop ();
            }
          else
            {
              failStartup (QAudio::InterruptedState == newState
                           ? tr ("The audio output device was interrupted during startup.")
                           : tr ("The audio output device stopped before startup confirmation."));
            }
        }
    }
  switch (newState) {
    case QAudio::IdleState: Q_EMIT status (tr ("Idle")); break;
    case QAudio::ActiveState: Q_EMIT status (tr ("Sending")); break;
    case QAudio::SuspendedState: Q_EMIT status (tr ("Suspended")); break;
#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
    case QAudio::InterruptedState:
      m_recreatePending = true;
      Q_EMIT status (tr ("Interrupted"));
      reportError (tr ("The audio output device was interrupted."));
      break;
#endif
    case QAudio::StoppedState:
      if(audioError ()) {
        m_recreatePending = true;
        Q_EMIT status (tr ("Error"));
      } else {
        Q_EMIT status (tr ("Stopped"));
      }
      break;
  }
}

void SoundOutput::handleStartupTimeout ()
{
  if (!m_startupPending || !m_stream) return;

  auto const state = m_stream->state ();
  auto const decision = AudioStartupPolicy::decide (
    QAudio::ActiveState == state,
    QAudio::StoppedState == state,
#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
    QAudio::InterruptedState == state,
#else
    false,
#endif
    m_startupElapsed.isValid () ? m_startupElapsed.elapsed () : 500, 500);
  if (AudioStartupPolicy::Decision::Confirm == decision)
    {
      confirmStartup ();
    }
  else
    {
      failStartup (tr ("Audio output startup timed out after 500 ms (state=%1, processedUSecs=%2).")
                   .arg (static_cast<int> (state))
                   .arg (m_stream->processedUSecs ()));
    }
}

void SoundOutput::handleNotify ()
{
  if (!m_stream || m_progressReported) return;
  auto const processed = m_stream->processedUSecs ();
  if (processed > m_startupProcessedUSecs)
    {
      m_progressReported = true;
      Q_EMIT startupProgress (
        tr ("Audio output progress device=%1 processedUSecs=%2 state=Active")
          .arg (m_device.deviceName ()).arg (processed));
    }
}

void SoundOutput::confirmStartup ()
{
  if (!m_startupPending || !m_stream) return;
  m_startupPending = false;
  m_startupTimer.stop ();
  m_recreatePending = false;
  Q_EMIT startupConfirmed (
    tr ("Audio startup confirmed device=%1 state=Active processedUSecs=%2")
      .arg (m_device.deviceName ()).arg (m_stream->processedUSecs ()));
  Q_EMIT ready ();
}

void SoundOutput::failStartup (QString const& reason)
{
  if (!m_startupPending) return;
  m_startupPending = false;
  m_startupTimer.stop ();
  m_recreatePending = true;
  if (m_stream && m_stream->state () != QAudio::StoppedState)
    m_stream->stop ();
  reportError (reason + tr (" device=%1 state=%2 processedUSecs=%3")
              .arg (m_device.deviceName ())
              .arg (m_stream ? static_cast<int> (m_stream->state ()) : -1)
              .arg (m_stream ? m_stream->processedUSecs () : 0));
}
