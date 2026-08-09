// -*- Mode: C++ -*-
#ifndef SOUNDOUT_H__
#define SOUNDOUT_H__

#include <QObject>
#include <QString>
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#include <QElapsedTimer>
#include <QTimer>

class QAudioDeviceInfo;

// An instance of this sends audio data to a specified soundcard.

class SoundOutput
  : public QObject
{
  Q_OBJECT;

public:
  SoundOutput ()
    : m_framesBuffered {0}
    , m_channels {0}
    , m_volume {1.0}
    , m_recreatePending {false}
    , m_errorReported {false}
    , m_startupPending {false}
    , m_startupProcessedUSecs {0}
    , m_progressReported {false}
  {
    m_startupTimer.setParent (this);
    m_startupTimer.setSingleShot (true);
    connect (&m_startupTimer, &QTimer::timeout, this, &SoundOutput::handleStartupTimeout);
  }

  qreal attenuation () const;

public Q_SLOTS:
  void setFormat (QAudioDeviceInfo const& device, unsigned channels, int frames_buffered = 0);
  bool restart (QIODevice *);
  void suspend ();
  void resume ();
  void reset ();
  void stop ();
  void setAttenuation (qreal);	/* unsigned */
  void resetAttenuation ();	/* to zero */

Q_SIGNALS:
  void error (QString message) const;
  void status (QString message) const;
  void ready () const;
  void startupConfirmed (QString message) const;
  void startupProgress (QString message) const;

private:
  int m_framesBuffered;
  unsigned m_channels;
  bool audioError ();
  bool recreateStream ();
  void reportError (QString const&);

private Q_SLOTS:
  void handleStateChanged (QAudio::State);
  void handleStartupTimeout ();
  void handleNotify ();

private:
  QScopedPointer<QAudioOutput> m_stream;
  QAudioDeviceInfo m_device;
  unsigned m_msBuffered;
  qreal m_volume;
  bool m_recreatePending;
  bool m_errorReported;
  QTimer m_startupTimer;
  QElapsedTimer m_startupElapsed;
  bool m_startupPending;
  qint64 m_startupProcessedUSecs;
  bool m_progressReported;

  void confirmStartup ();
  void failStartup (QString const& reason);
};

#endif
