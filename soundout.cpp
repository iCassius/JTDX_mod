#include "soundout.h"

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QSysInfo>
#include <qmath.h>
#include <QDebug>

#include "moc_soundout.cpp"

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
  //      qDebug() << "A" << m_volume << m_stream->notifyInterval();
}

bool SoundOutput::recreateStream ()
{
  QAudioDeviceInfo device;
  bool found {false};

  Q_FOREACH (auto const& candidate, QAudioDeviceInfo::availableDevices (QAudio::AudioOutput))
    {
      // The device handle can become stale after a Windows endpoint reset.
      // Keep the configured route, but bind it to the refreshed endpoint.
      if (candidate == m_device || candidate.deviceName () == m_device.deviceName ())
        {
          device = candidate;
          found = true;
          break;
        }
    }

  if (!found)
    {
      reportError (tr ("The configured audio output device is not available."));
      return false;
    }

  setFormat (device, m_channels, m_framesBuffered);
  return m_stream && !m_recreatePending;
}

void SoundOutput::restart (QIODevice * source)
{
  // Allow each requested transmission to report its own startup failure.
  m_errorReported = false;

#if defined (Q_OS_WIN)
  // Windows endpoint handles can remain apparently healthy after an idle USB
  // reset while silently producing no samples.  Re-enumerate and recreate the
  // output object before each real transmission start.
  if (!recreateStream ())
    {
      return;
    }
#else
  if (m_recreatePending || !m_stream || QAudio::NoError != m_stream->error ())
    {
      if (!recreateStream ())
        {
          return;
        }
    }
#endif

  if (!m_stream)
    {
      reportError (tr ("The audio output device could not be initialized."));
      return;
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
  m_stream->start (source);
  if (audioError ())
    {
      m_recreatePending = true;
      return;
    }
  if (QAudio::StoppedState == m_stream->state ())
    {
      m_recreatePending = true;
      reportError (tr ("The audio output device stopped immediately after startup."));
      return;
    }
  Q_EMIT ready ();
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
