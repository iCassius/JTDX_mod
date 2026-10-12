#include "mainwindow.h"
#include "Bands.hpp"
#include "soundout.h"
#include "JtdxWebService.hpp"
#include "MetaDataRegistry.hpp"
#include "revision_utils.hpp"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMetaMethod>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSharedMemory>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <limits>

#ifndef P0_TX_MAINWINDOW_INTEGRATION_TEST
# error This test must be built with the private MainWindow isolation macro.
#endif
#ifndef P0_TX_MAINWINDOW_TEST_ALLOWED_ROOT
# error This test must be given its isolated local-support runtime root.
#endif

extern qint32 g_iptt;

class MainWindowP0IntegrationTestAccess
{
public:
  struct SignalCounts
  {
    int audio_input_start {0};
    int audio_output_initialize {0};
    int modulator_start {0};
    int config_updates {0};
    int ptt_on_updates {0};
    int gui_timer_timeouts {0};
    int ptt_off_timer_timeouts {0};
    int ptt_off_updates {0};
  };

  static QTimer * gui_timer (MainWindow& window)
  {
    return &window.m_guiTimer;
  }

  static QTimer * ptt_off_timer (MainWindow& window) { return &window.ptt0Timer; }
  static int ptt_off_timer_interval (MainWindow const& window) { return window.ptt0Timer.interval (); }
  static bool ptt_off_timer_active (MainWindow const& window) { return window.ptt0Timer.isActive (); }
  static bool ptt_on_timer_active (MainWindow const& window) { return window.ptt1Timer.isActive (); }
  static bool tune_timer_active (MainWindow const& window) { return window.tuneButtonTimer.isActive (); }

  static bool safe_configuration (MainWindow const& window)
  {
    auto const& config = window.m_config;
    return config.is_dummy_rig ()
      && QStringLiteral ("None") == config.rig_name ()
      && !config.web_ui_enabled ()
      && !config.spot_to_psk_reporter ()
      && !config.accept_udp_requests ()
      && !config.enable_udp1_adif_sending ()
      && !config.enable_udp2_broadcast ()
      && !config.enable_tcp_connection ()
      && (!window.m_webService || !window.m_webService->is_listening ())
      && QProcess::NotRunning == window.proc_jtdxjt9.state ();
  }

  static bool tx_idle (MainWindow const& window)
  {
    return !window.m_transmitting && !window.m_tune && !window.m_btxok
      && !window.m_tx_when_ready && 0 == g_iptt;
  }

  static bool tx_active (MainWindow const& window)
  {
    return window.m_enableTx && window.m_transmitting && window.m_btxok && 1 == g_iptt
      && window.m_rigState.online () && window.m_rigState.ptt () && !window.m_tune;
  }
  static bool pending_ptt_start (MainWindow const& window)
  {
    return 1 == g_iptt && window.m_tx_when_ready && window.m_pttStartPolicy.active ()
      && window.ptt1Timer.isActive ();
  }
  static bool transmitting (MainWindow const& window) { return window.m_transmitting; }
  static bool btxok (MainWindow const& window) { return window.m_btxok; }
  static bool tx_when_ready (MainWindow const& window) { return window.m_tx_when_ready; }
  static int decoder_state (MainWindow const& window) { return window.proc_jtdxjt9.state (); }

  static bool tune_active (MainWindow const& window)
  {
    return window.m_tune && window.m_transmitting && window.m_btxok && 1 == g_iptt
      && window.m_rigState.online () && window.m_rigState.ptt ();
  }
  static bool tune_flag (MainWindow const& window) { return window.m_tune; }

  static bool enable_tx (MainWindow const& window) { return window.m_enableTx; }
  static bool tx_first (MainWindow const& window) { return window.m_txFirst; }
  static bool red_tx_residual (MainWindow const& window)
  {
    return window.m_transmitting || window.m_btxok || window.m_tx_when_ready
      || 0 != g_iptt
      || window.tx_status_label->styleSheet ().contains (QStringLiteral ("#ffff33"));
  }
  static QString tx_status_text (MainWindow const& window) { return window.tx_status_label->text (); }
  static quint64 generation (MainWindow const& window)
  {
    return window.m_config.transceiver_session_generation ();
  }
  static quint64 state_generation (MainWindow const& window) { return window.m_rigStateGeneration; }
  static quint64 token_serial (MainWindow const& window) { return window.m_pttRequestToken.serial; }
  static quint64 token_generation (MainWindow const& window) { return window.m_pttRequestToken.generation; }
  static bool token_active (MainWindow const& window) { return window.m_pttStartPolicy.active (); }
  static bool rig_state_online (MainWindow const& window) { return window.m_rigState.online (); }
  static bool rig_state_ptt (MainWindow const& window) { return window.m_rigState.ptt (); }
  static QString rig_control_log_path (MainWindow const& window)
  {
    return window.m_dataDir.absoluteFilePath (QStringLiteral ("jtdx_recovery.log"));
  }
  static QString data_directory (MainWindow const& window) { return window.m_dataDir.absolutePath (); }
  static qint64 virtual_now_ms (MainWindow const& window)
  {
    return window.m_jtdxtime->currentMSecsSinceEpoch2 ();
  }
  static void set_virtual_time_ms (MainWindow& window, qint64 target)
  {
    auto const system_now = QDateTime::currentMSecsSinceEpoch ();
    window.m_jtdxtime->SetOffset (static_cast<float> (target - system_now) / 1000.0F);
  }
  static void rig_open (MainWindow& window) { window.rigOpen (); }
  static void request_config_ptt_off (MainWindow& window) { window.m_config.transceiver_ptt (false); }
  static bool change_poll_interval (MainWindow& window, int * before, int * after)
  {
    auto * receiver = configuration_impl_receiver (window);
    auto * spin = receiver ? receiver->findChild<QSpinBox *> (
      QStringLiteral ("CAT_poll_interval_spin_box")) : nullptr;
    if (!spin) return false;
    auto const previous = spin->value ();
    if (before) *before = previous;
    auto const next = previous < spin->maximum () ? previous + 1 : previous - 1;
    spin->setValue (next);
    if (after) *after = spin->value ();
    return spin->value () != previous;
  }
  static void activate_ft8 (MainWindow& window)
  {
    window.on_actionFT8_triggered ();
    window.genCQMsg ();
    window.ui->tx6->setText (QStringLiteral ("CQ %1 %2")
                             .arg (window.m_config.my_callsign ())
                             .arg (window.m_config.my_grid ().left (4)));
    window.on_txb6_clicked ();
  }
  static void set_tx_first_via_button (MainWindow& window, bool value)
  {
    if (window.m_txFirst != value) window.ui->TxMinuteButton->click ();
  }
  static void enable_tx_via_button (MainWindow& window, bool value)
  {
    if (window.m_enableTx != value) window.ui->enableTxButton->click ();
  }
  static void manual_stop_via_button (MainWindow& window) { window.ui->stopTxButton->click (); }
  static void tune_via_button (MainWindow& window) { window.ui->tuneButton->click (); }
  static bool configuration_has_safe_endpoints (MainWindow const& window)
  {
    auto const& config = window.m_config;
    QSettings settings {window.m_settings->fileName (), window.m_settings->format ()};
    settings.beginGroup (QStringLiteral ("Configuration"));
    auto const method = settings.value (QStringLiteral ("PTTMethod"))
      .value<TransceiverFactory::PTTMethod> ();
    bool const endpoints_empty = settings.value (QStringLiteral ("CATSerialPort")).toString ().isEmpty ()
      && settings.value (QStringLiteral ("CATNetworkPort")).toString ().isEmpty ()
      && settings.value (QStringLiteral ("CATUSBPort")).toString ().isEmpty ()
      && settings.value (QStringLiteral ("CATTCIPort")).toString ().isEmpty ()
      && settings.value (QStringLiteral ("PTTport")).toString ().isEmpty ()
      && !settings.value (QStringLiteral ("CATForceDTR")).toBool ()
      && !settings.value (QStringLiteral ("CATForceRTS")).toBool ()
      && !settings.value (QStringLiteral ("DTR")).toBool ()
      && !settings.value (QStringLiteral ("RTS")).toBool ();
    settings.endGroup ();
    return config.is_dummy_rig () && QStringLiteral ("None") == config.rig_name ()
      && method == TransceiverFactory::PTT_method_VOX && endpoints_empty;
  }
  static void print_endpoint_state (MainWindow const& window)
  {
    QSettings settings {window.m_settings->fileName (), window.m_settings->format ()};
    settings.beginGroup (QStringLiteral ("Configuration"));
    auto const method = settings.value (QStringLiteral ("PTTMethod"))
      .value<TransceiverFactory::PTTMethod> ();
    std::fprintf (stderr,
                  "DETAIL SAFE_ENDPOINTS method_is_vox=%d serial='%s' network='%s' usb='%s' tci='%s' ptt='%s' force_dtr=%d force_rts=%d dtr=%d rts=%d\n",
                  method == TransceiverFactory::PTT_method_VOX,
                  qPrintable (settings.value (QStringLiteral ("CATSerialPort")).toString ()),
                  qPrintable (settings.value (QStringLiteral ("CATNetworkPort")).toString ()),
                  qPrintable (settings.value (QStringLiteral ("CATUSBPort")).toString ()),
                  qPrintable (settings.value (QStringLiteral ("CATTCIPort")).toString ()),
                  qPrintable (settings.value (QStringLiteral ("PTTport")).toString ()),
                  settings.value (QStringLiteral ("CATForceDTR")).toBool (),
                  settings.value (QStringLiteral ("CATForceRTS")).toBool (),
                  settings.value (QStringLiteral ("DTR")).toBool (),
                  settings.value (QStringLiteral ("RTS")).toBool ());
    settings.endGroup ();
  }
  static QObject * configuration_impl_receiver (MainWindow& window)
  {
    for (auto * object : window.findChildren<QObject *> ())
      if (QByteArrayLiteral ("Configuration::impl") == object->metaObject ()->className ())
        return object;
    return nullptr;
  }
  static bool queue_stale_configuration_update (MainWindow& window, QString * signature)
  {
    auto * receiver = configuration_impl_receiver (window);
    if (!receiver) return false;
    auto const * meta = receiver->metaObject ();
    for (int i = 0; i < meta->methodCount (); ++i)
      {
        auto const method = meta->method (i);
        if (QByteArrayLiteral ("handle_transceiver_update") != method.name ()
            || 3 != method.parameterCount ()) continue;
        auto const types = method.parameterTypes ();
        if (3 != types.size ()) continue;
        if (signature)
          *signature = QString::fromLatin1 (meta->className ()) + QLatin1Char (' ')
            + QString::fromLatin1 (method.methodSignature ());
        Transceiver::TransceiverState stale_state;
        stale_state.online (true);
        stale_state.ptt (true);
        unsigned sequence {std::numeric_limits<unsigned>::max ()};
        auto const stale_generation = window.m_config.transceiver_session_generation () - 1;
        return method.invoke (
          receiver, Qt::QueuedConnection, QGenericReturnArgument (),
          QGenericArgument (types.at (0).constData (), &stale_state),
          QGenericArgument (types.at (1).constData (), &sequence),
          QGenericArgument (types.at (2).constData (), &stale_generation));
      }
    return false;
  }

  static bool transceiver_online (MainWindow const& window)
  {
    return window.m_config.is_transceiver_online ();
  }

  static bool monitoring (MainWindow const& window)
  {
    return window.m_monitoring;
  }

  static void print_state (MainWindow const& window, char const * label)
  {
    auto const& config = window.m_config;
    std::fprintf (stderr,
                  "DETAIL %s rig_dummy=%d rig_name=%s online=%d audio_in_null=%d audio_in=%s audio_out_null=%d audio_out=%s web=%d psk=%d accept_udp=%d udp1=%d udp2=%d tcp=%d web_listening=%d decoder_state=%d tx=%d tune=%d btxok=%d tx_when_ready=%d iptt=%d monitoring=%d\n",
                  label,
                  config.is_dummy_rig (), qPrintable (config.rig_name ()),
                  config.is_transceiver_online (),
                  config.audio_input_device ().isNull (), qPrintable (config.audio_input_device ().deviceName ()),
                  config.audio_output_device ().isNull (), qPrintable (config.audio_output_device ().deviceName ()),
                  config.web_ui_enabled (), config.spot_to_psk_reporter (),
                  config.accept_udp_requests (), config.enable_udp1_adif_sending (),
                  config.enable_udp2_broadcast (), config.enable_tcp_connection (),
                  window.m_webService && window.m_webService->is_listening (),
                  window.proc_jtdxjt9.state (), window.m_transmitting, window.m_tune,
                  window.m_btxok, window.m_tx_when_ready, g_iptt, window.m_monitoring);
  }

  static void print_stale_snapshot (MainWindow const& window, char const * label, int sends)
  {
    std::fprintf (stderr,
                  "DETAIL STALE_REOPEN %s tx=%d tune=%d btxok=%d ready=%d iptt=%d token_active=%d token_serial=%llu token_gen=%llu rig_gen=%llu state_gen=%llu ptt1=%d ptt0=%d online=%d rig_ptt=%d sends=%d\n",
                  label, window.m_transmitting, window.m_tune, window.m_btxok,
                  window.m_tx_when_ready, g_iptt, window.m_pttStartPolicy.active (),
                  static_cast<unsigned long long> (window.m_pttRequestToken.serial),
                  static_cast<unsigned long long> (window.m_pttRequestToken.generation),
                  static_cast<unsigned long long> (window.m_config.transceiver_session_generation ()),
                  static_cast<unsigned long long> (window.m_rigStateGeneration),
                  window.ptt1Timer.isActive (), window.ptt0Timer.isActive (),
                  window.m_config.is_transceiver_online (), window.m_rigState.ptt (), sends);
  }

  static void observe_signals (MainWindow& window, SignalCounts& counts)
  {
    QObject::connect (&window, &MainWindow::startAudioInputStream, &window,
                      [&counts] (QAudioDeviceInfo const&, int, AudioDevice*, unsigned,
                                 AudioDevice::Channel) {++counts.audio_input_start;});
    QObject::connect (&window, &MainWindow::initializeAudioOutputStream, &window,
                      [&counts] (QAudioDeviceInfo, unsigned, unsigned) {++counts.audio_output_initialize;});
    QObject::connect (&window, &MainWindow::sendMessage, &window,
                      [&counts] (unsigned, double, double, double, SoundOutput*, AudioDevice::Channel,
                                 bool, double, int) {++counts.modulator_start;});
    QObject::connect (&window.m_config, &Configuration::transceiver_update, &window,
                      [&window, &counts] (Transceiver::TransceiverState const& state) {
                        ++counts.config_updates;
                        if (state.online () && state.ptt ()) ++counts.ptt_on_updates;
                        if (state.online () && !state.ptt ()
                            && window.m_config.transceiver_session_generation ()
                                 == window.m_rigStateGeneration)
                          ++counts.ptt_off_updates;
                      });
    QObject::connect (&window.m_guiTimer, &QTimer::timeout, &window,
                      [&counts] {++counts.gui_timer_timeouts;});
    QObject::connect (&window.ptt0Timer, &QTimer::timeout, &window,
                      [&counts] {++counts.ptt_off_timer_timeouts;});
  }

  static void observe_finished (MainWindow& window, bool& finished)
  {
    QObject::connect (&window, &MainWindow::finished, &window,
                      [&finished] { finished = true; });
  }

  static void stop (MainWindow& window)
  {
    window.stopTx ();
  }

  static void reset_auto_seq_retry_fixture (MainWindow& window,
                                           bool singleShot, bool skipTx1,
                                           bool operatorHold)
  {
    window.ptt1Timer.stop ();
    window.ptt0Timer.stop ();
    window.cancelPttStartAndRevokeAdmission (QStringLiteral ("test_fixture_reset"));
    window.m_pttStartPolicy.cancel ();
    window.m_pttRequestToken = {};
    window.m_pttRequestContext = {};
    window.m_tx_when_ready = false;
    window.m_bTxTime = false;
    window.m_transmitting = false;
    window.m_btxok = false;
    window.m_tune = false;
    window.m_tuneStopPending = false;
    window.m_restart = false;
    window.m_txNext = false;
    window.m_haltTrans = false;
    window.m_enableTx = false;
    window.ui->enableTxButton->setChecked (false);
    g_iptt = 0;

    window.m_autoSeqInitialCallStall.cancel ();
    window.m_autoSeqInitialCallOperatorHoldBlockedCall.clear ();
    window.m_autoSeqStallSuppressedCall.clear ();
    window.m_autoSeqStallTargetedReplySeen = false;
    window.m_autoSeqStallReplySender.clear ();
    window.m_autoSeqStallSkipCurrentBatch = false;
    window.m_autoSeqLiveDecodeBatchPending = false;
    window.m_autoSeqLastAttemptCall.clear ();
    window.m_autoSeqLastAttemptId = 0;
    window.m_autoSeqAttemptSequence = 0;
    window.m_autoSeqInitialCallOperatorHold = operatorHold;
    window.m_autoSeqInternalClear = false;
    window.m_autoSeqInternalHalt = false;
    window.m_autoSeqInternalTargetSelection = true;
    window.ui->dxCallEntry->clear ();
    window.m_autoSeqInternalTargetSelection = false;
    window.m_hisCall.clear ();
    window.m_hisGrid.clear ();

    window.m_qsoHistory.init ();
    window.m_qsoHistory.owndata (QStringLiteral ("NA"), QStringLiteral ("K"),
                                 QStringLiteral ("FN20"), true);
    window.m_autoseq = true;
    window.m_callMode = 1;
    window.m_houndMode = false;
    window.m_singleshot = singleShot;
    window.m_skipTx1 = skipTx1;
    window.m_config.setAutoCallNewDXCC (true);
    window.m_config.setAutoAnswerDirectedCalls (false);
    window.m_config.revoke_transceiver_ptt_start ();
  }

  static void seed_auto_retry_target (MainWindow& window, QString const& call,
                                      int attempts, unsigned receiveTime)
  {
    window.m_qsoHistory.time (receiveTime);
    for (int i = 0; i < attempts; ++i)
      window.m_qsoHistory.message (call, QsoHistory::SCALL, 22, QString {},
                                   QStringLiteral ("S"), QStringLiteral ("EU"),
                                   QStringLiteral ("DL"), receiveTime, QString {},
                                   1500, QStringLiteral ("FT8"));
    window.m_qsoHistory.message (call, QsoHistory::RFIN, 22, QStringLiteral ("JO31"),
                                 QStringLiteral ("DX"), QStringLiteral ("EU"),
                                 QStringLiteral ("DL"), receiveTime,
                                 QStringLiteral ("-60"), 1500, QStringLiteral ("FT8"));
    window.m_autoSeqInternalTargetSelection = true;
    window.ui->dxCallEntry->setText (call);
    window.m_autoSeqInternalTargetSelection = false;
    window.m_hisCall = call;
    window.m_autoSeqInitialCallStall.select (
        call.toStdString (), Radio::base_callsign (call).toStdString (), -60,
        window.m_config.bands ()->find (window.m_freqNominal).toStdString (),
        window.m_mode.toStdString (), 1,
        MainWindowP0IntegrationTestAccess::steady_clock_now_ms ());
    window.m_autoSeqAttemptSequence = 1;
    window.m_autoSeqLastAttemptId = 1;
    window.m_autoSeqLastAttemptCall = call;
  }

  static void seed_auto_candidate (MainWindow& window, QString const& call,
                                   unsigned receiveTime)
  {
    auto const add_decoded_candidate = [] (QsoHistory& history,
                                           QString const& decodedCall,
                                           unsigned decodedAt) {
      history.time (decodedAt);
      history.message (decodedCall, QsoHistory::RFIN, 22, QStringLiteral ("JO32"),
                       QStringLiteral ("DX"), QStringLiteral ("EU"),
                       QStringLiteral ("DL"), decodedAt,
                       QStringLiteral ("-60"), 1700, QStringLiteral ("FT8"));
    };
    add_decoded_candidate (window.m_qsoHistory, call, receiveTime);
  }
  static QsoHistory auto_seq_history_copy (MainWindow const& window)
  { return window.m_qsoHistory; }
  static void seed_auto_candidate (QsoHistory& history, QString const& call,
                                   unsigned receiveTime)
  {
    history.time (receiveTime);
    history.message (call, QsoHistory::RFIN, 22, QStringLiteral ("JO32"),
                     QStringLiteral ("DX"), QStringLiteral ("EU"),
                     QStringLiteral ("DL"), receiveTime,
                     QStringLiteral ("-60"), 1700, QStringLiteral ("FT8"));
  }
  static bool auto_seq_selects_rare_candidate (QsoHistory& history,
                                                QString const& expectedCall,
                                                unsigned receiveTime)
  {
    history.time (receiveTime);
    QString call;
    QString grid;
    QString report;
    QString mode;
    unsigned options = QsoHistory::AutoCallNewDXCC;
    int rx = 0;
    int tx = 1700;
    int count = 0;
    int priority = 0;
    auto const status = history.autoseq (call, grid, report, rx, tx, options,
                                         count, priority, mode);
    return call == expectedCall
      && (status == QsoHistory::RCQ || status == QsoHistory::RFIN)
      && priority == 22;
  }
  static void prime_pending_tx_request (MainWindow& window)
  {
    window.m_enableTx = true;
    window.ui->enableTxButton->setChecked (true);
    window.m_btxok = true;
    window.m_bTxTime = true;
    window.m_tx_when_ready = true;
    window.m_pttRequestToken = window.m_pttStartPolicy.begin (
        window.m_config.transceiver_session_generation ());
    window.ptt1Timer.start (30000);
  }
  static void prime_active_software_tx (MainWindow& window)
  {
    // Model the software state seen in the incident without opening the
    // Dummy rig or starting the audio/modulator paths.
    window.m_enableTx = true;
    window.ui->enableTxButton->setChecked (true);
    window.m_btxok = true;
    window.m_bTxTime = true;
    window.m_tx_when_ready = false;
    window.m_transmitting = true;
    window.m_monitoroff = true;
    g_iptt = 1;
  }
  static void prime_manual_enable_tx (MainWindow& window)
  {
    window.m_enableTx = true;
    window.ui->enableTxButton->setChecked (true);
  }

  static void begin_fresh_auto_seq_batch (MainWindow& window, qint64 decodeStarted)
  {
    ++window.m_autoSeqDecodeBatchId;
    window.m_msDecStarted = decodeStarted;
    window.m_autoSeqLiveDecodeBatchStartedMs = decodeStarted;
    window.m_autoSeqLiveDecodeBatchPending = true;
    window.m_autoSeqStallSkipCurrentBatch = false;
  }

  static void process_auto (MainWindow& window) { window.process_Auto (); }
  static void request_rx_stop_for_test (MainWindow& window)
  {
    // The test injects only the pending RX stop marker; guiUpdate() and its
    // real stopTx()/200 ms OFF-timer path perform the cleanup.
    window.m_haltTrans = true;
    window.m_tuneStopPending = true;
    window.m_monitoroff = true;
  }
  static void drive_gui_update (MainWindow& window) { window.guiUpdate (); }
  static bool operator_hold (MainWindow const& window)
  { return window.m_autoSeqInitialCallOperatorHold; }
  static bool batch_skipped (MainWindow const& window)
  { return window.m_autoSeqStallSkipCurrentBatch; }
  static bool auto_sequence_enabled (MainWindow const& window)
  { return window.m_autoseq; }
  static qint64 steady_clock_now_ms () noexcept
  {
    return std::chrono::duration_cast<std::chrono::milliseconds> (
        std::chrono::steady_clock::now ().time_since_epoch ()).count ();
  }
  static bool retry_threshold_preconditions (MainWindow const& window,
                                            int expectedRawRetryCount,
                                            int expectedEffectiveRetryCount)
  {
    QsoHistory::Status history_status = QsoHistory::NONE;
    int retry_count = -1;
    auto const owner = QString::fromStdString (window.m_autoSeqInitialCallStall.call ());
    return window.m_autoSeqInitialCallStall.active ()
      && window.m_autoSeqLiveDecodeBatchPending
      && !window.m_manualDecode && !window.m_diskData
      && window.m_autoSeqInitialCallStall.elapsed_ms (steady_clock_now_ms ())
           < AutoSeqInitialCallStallPolicy::timeout_ms
      && window.m_qsoHistory.diagnosticSnapshot (owner, history_status, retry_count)
      && history_status == QsoHistory::RFIN
      && retry_count == expectedRawRetryCount
      && retry_count - (window.m_transmitting ? 1 : 0) == expectedEffectiveRetryCount
      && window.m_config.answerCQCount ()
      && window.m_config.nAnswerCQCounter () == 2
      && expectedEffectiveRetryCount >= window.m_config.nAnswerCQCounter ();
  }
  static bool stall_owner_active (MainWindow const& window)
  { return window.m_autoSeqInitialCallStall.active (); }
  static QString stall_owner_call (MainWindow const& window)
  { return QString::fromStdString (window.m_autoSeqInitialCallStall.call ()); }
  static QString current_dx_call (MainWindow const& window)
  { return window.m_hisCall; }
  static QString suppressed_call (MainWindow const& window)
  { return window.m_autoSeqStallSuppressedCall; }
  static bool ptt_token_active (MainWindow const& window)
  { return window.m_pttStartPolicy.active (); }
  static bool retry_states_clear (MainWindow const& window)
  {
    return !window.m_enableTx && !window.m_transmitting && !window.m_btxok
      && !window.m_tx_when_ready && !window.m_tune && 0 == g_iptt
      && !window.ptt1Timer.isActive () && !window.m_pttStartPolicy.active ();
  }
  static void operator_clear_dx (MainWindow& window)
  { window.clearDX (QStringLiteral (" cleared by integration test operator action")); }
  static void operator_disable_auto_seq (MainWindow& window)
  { window.on_AutoSeqButton_clicked (false); }
};

namespace
{
  bool check (bool condition, char const * message)
  {
    if (!condition)
      {
        std::fprintf (stderr, "FAIL: %s\n", message);
      }
    return condition;
  }

  bool make_environment_directories ()
  {
    auto const root = QDir::cleanPath (QString::fromUtf8 (P0_TX_MAINWINDOW_TEST_ALLOWED_ROOT));
    auto const normalized = [] (QString const& path) {
      return QDir::cleanPath (QFileInfo {path}.absoluteFilePath ());
    };
    auto const same_path = [] (QString const& actual, QString const& expected) {
#if defined (Q_OS_WIN)
      return 0 == QString::compare (actual, expected, Qt::CaseInsensitive);
#else
      return actual == expected;
#endif
    };
    auto const temp = normalized (qEnvironmentVariable ("TEMP"));
    auto const tmp = normalized (qEnvironmentVariable ("TMP"));
    auto const appdata = normalized (qEnvironmentVariable ("APPDATA"));
    auto const local_appdata = normalized (qEnvironmentVariable ("LOCALAPPDATA"));
    if (!same_path (temp, QDir {root}.absoluteFilePath (QStringLiteral ("TEMP")))
        || !same_path (tmp, QDir {root}.absoluteFilePath (QStringLiteral ("TMP")))
        || !same_path (appdata, QDir {root}.absoluteFilePath (QStringLiteral ("APPDATA")))
        || !same_path (local_appdata, QDir {root}.absoluteFilePath (QStringLiteral ("LOCALAPPDATA"))))
      {
        std::fprintf (stderr, "FAIL: runtime paths must exactly match the isolated local-support test root\n");
        return false;
      }
    return QDir {}.mkpath (temp) && QDir {}.mkpath (tmp)
      && QDir {}.mkpath (appdata) && QDir {}.mkpath (local_appdata);
  }

  bool wait_until (std::function<bool ()> const& predicate, int timeout_ms)
  {
    QElapsedTimer elapsed;
    elapsed.start ();
    while (!predicate () && elapsed.elapsed () < timeout_ms)
      {
        QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
        QThread::msleep (2);
      }
    QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
    return predicate ();
  }

  void pump_for (int duration_ms)
  {
    QElapsedTimer elapsed;
    elapsed.start ();
    while (elapsed.elapsed () < duration_ms)
      {
        QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
        QThread::msleep (2);
      }
  }

  qint64 next_slot_boundary (qint64 after_ms, bool tx_first)
  {
    qint64 const phase_seconds = tx_first ? 0 : 15;
    qint64 const now_seconds = after_ms / 1000;
    qint64 const slot_number = (now_seconds - phase_seconds) / 30 + 1;
    return (slot_number * 30 + phase_seconds) * 1000;
  }

  bool check_log_event (QString const& log, QString const& event,
                        QString const& required_fields, char const * label)
  {
    auto const marker = QStringLiteral ("stop_diag event=%1 ").arg (event);
    auto const position = log.lastIndexOf (marker);
    if (position < 0)
      {
        std::fprintf (stderr, "FAIL: %s log event missing: %s\n",
                      label, qPrintable (marker));
        return false;
      }
    auto const line_end = log.indexOf (QLatin1Char ('\n'), position);
    auto const line = log.mid (position, line_end < 0 ? -1 : line_end - position);
    std::fprintf (stderr, "DETAIL LOG %s %s\n", label, qPrintable (line));
    bool ok = true;
    for (auto const& field : required_fields.split (QLatin1Char ('|'), Qt::SkipEmptyParts))
      if (!line.contains (field))
        {
          ok = false;
          std::fprintf (stderr, "FAIL: %s missing field: %s\n", label, qPrintable (field));
        }
    return ok;
  }

  QString read_log_from (QString const& path, qint64 offset)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly)) return {};
    if (offset < 0 || !file.seek (offset)) return {};
    return QString::fromUtf8 (file.readAll ());
  }

  bool verify_idle_rx (MainWindow const& window,
                       MainWindowP0IntegrationTestAccess::SignalCounts const& counts,
                       bool expected_enable_tx, char const * label)
  {
    auto const idle = MainWindowP0IntegrationTestAccess::tx_idle (window)
      && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
      && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window);
    std::fprintf (stderr,
                  "DETAIL RX %s enabled=%d online=%d monitoring=%d tx_status='%s' gui_ticks=%d\n",
                  label, MainWindowP0IntegrationTestAccess::enable_tx (window),
                  MainWindowP0IntegrationTestAccess::transceiver_online (window),
                  MainWindowP0IntegrationTestAccess::monitoring (window),
                  qPrintable (MainWindowP0IntegrationTestAccess::tx_status_text (window)), counts.gui_timer_timeouts);
    return check (idle && MainWindowP0IntegrationTestAccess::enable_tx (window) == expected_enable_tx
                  && MainWindowP0IntegrationTestAccess::transceiver_online (window)
                  && !MainWindowP0IntegrationTestAccess::red_tx_residual (window), label);
  }

  bool run_ft8_cycle (MainWindow& window,
                      MainWindowP0IntegrationTestAccess::SignalCounts& counts,
                      bool tx_first, int cycle_index, bool continuous_rx)
  {
    auto const log_path = MainWindowP0IntegrationTestAccess::rig_control_log_path (window);
    auto const start_log_offset = QFileInfo {log_path}.size ();
    auto const label = QStringLiteral ("phase_%1_cycle_%2")
      .arg (tx_first ? QStringLiteral ("00_30") : QStringLiteral ("15_45"))
      .arg (cycle_index);
    MainWindowP0IntegrationTestAccess::set_tx_first_via_button (window, tx_first);
    MainWindowP0IntegrationTestAccess::enable_tx_via_button (window, true);
    auto const boundary = next_slot_boundary (
      MainWindowP0IntegrationTestAccess::virtual_now_ms (window), tx_first);
    MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, boundary + 1500);
    auto const sends_before = counts.modulator_start;
    std::fprintf (stderr,
                  "DETAIL SLOT %s boundary_epoch_ms=%lld start_offset_ms=1500 virtual_time_offset_seconds=%.3f\n",
                  qPrintable (label), static_cast<long long> (boundary),
                  (MainWindowP0IntegrationTestAccess::virtual_now_ms (window)
                   - QDateTime::currentMSecsSinceEpoch ()) / 1000.0);
    bool const started = wait_until ([&] {
      return counts.modulator_start > sends_before
        && MainWindowP0IntegrationTestAccess::tx_active (window);
    }, 7000);
    std::fprintf (stderr,
                  "DETAIL TX_START %s started=%d enable=%d tx=%d btxok=%d iptt=%d ready=%d token_active=%d token_serial=%llu token_gen=%llu rig_gen=%llu state_gen=%llu ptt1_active=%d sends=%d\n",
                  qPrintable (label), started,
                  MainWindowP0IntegrationTestAccess::enable_tx (window),
                  MainWindowP0IntegrationTestAccess::transmitting (window),
                  MainWindowP0IntegrationTestAccess::btxok (window), g_iptt,
                  MainWindowP0IntegrationTestAccess::tx_when_ready (window),
                  MainWindowP0IntegrationTestAccess::token_active (window),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::token_serial (window)),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::token_generation (window)),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::generation (window)),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::state_generation (window)),
                  MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window),
                  counts.modulator_start);
    bool ok = check (started && counts.modulator_start == sends_before + 1,
                     "actual guiUpdate begin gate, Dummy PTT state, PTT timer, startTx2 and sendMessage chain reached TX");
    ok &= check (MainWindowP0IntegrationTestAccess::generation (window)
                 == MainWindowP0IntegrationTestAccess::state_generation (window)
                 && MainWindowP0IntegrationTestAccess::token_generation (window)
                      == MainWindowP0IntegrationTestAccess::generation (window),
                 "TX request token and MainWindow state belong to the active Dummy rig generation");
    ok &= check (MainWindowP0IntegrationTestAccess::enable_tx (window),
                 "EnableTX remains open while the genuine scheduled slot begins");

    MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, boundary + 13800);
    QElapsedTimer real_timer_elapsed;
    real_timer_elapsed.start ();
    bool const stopped = wait_until ([&] {
      return MainWindowP0IntegrationTestAccess::tx_idle (window)
        && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window);
    }, 2000);
    auto const active_remaining = MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
      ? MainWindowP0IntegrationTestAccess::ptt_off_timer (window)->remainingTime () : -1;
    std::fprintf (stderr,
                  "DETAIL TX_END %s stopped=%d ptt0_active=%d interval=%d remaining=%d enable=%d tx=%d btxok=%d iptt=%d wall_elapsed_ms=%lld\n",
                  qPrintable (label), stopped,
                  MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window),
                  MainWindowP0IntegrationTestAccess::ptt_off_timer_interval (window), active_remaining,
                  MainWindowP0IntegrationTestAccess::enable_tx (window),
                  MainWindowP0IntegrationTestAccess::transmitting (window),
                  MainWindowP0IntegrationTestAccess::btxok (window), g_iptt,
                  static_cast<long long> (real_timer_elapsed.elapsed ()));
    ok &= check (stopped && MainWindowP0IntegrationTestAccess::enable_tx (window)
                 && MainWindowP0IntegrationTestAccess::ptt_off_timer_interval (window) == 200,
                 "actual FT8 TX slot end enters real stopTx and schedules the production 200ms ptt0Timer");
    auto const dispatch_baseline = counts.ptt_off_timer_timeouts;
    bool const off_dispatched = wait_until ([&] {
      return counts.ptt_off_timer_timeouts > dispatch_baseline
        && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
        && !MainWindowP0IntegrationTestAccess::rig_state_ptt (window);
    }, 2500);
    std::fprintf (stderr,
                  "DETAIL OFF_DISPATCH %s dispatched=%d timer_timeouts=%d same_gen=%d online=%d ptt=%d off_updates=%d\n",
                  qPrintable (label), off_dispatched, counts.ptt_off_timer_timeouts,
                  MainWindowP0IntegrationTestAccess::state_generation (window)
                    == MainWindowP0IntegrationTestAccess::generation (window),
                  MainWindowP0IntegrationTestAccess::rig_state_online (window),
                  MainWindowP0IntegrationTestAccess::rig_state_ptt (window), counts.ptt_off_updates);
    ok &= check (off_dispatched
                 && MainWindowP0IntegrationTestAccess::state_generation (window)
                      == MainWindowP0IntegrationTestAccess::generation (window)
                 && MainWindowP0IntegrationTestAccess::rig_state_online (window),
                 "200ms timer dispatched real stopTx2 and a current-generation software OFF state was observed");
    pump_for (100);
    auto const log = read_log_from (log_path, start_log_offset);
    ok &= check_log_event (
      log, QStringLiteral ("tx_slot_end"),
      QStringLiteral ("pre_enable_tx=1|pre_btxok=0|pre_transmitting=1|pre_tune=0|"
                      "pre_tx_when_ready=0|pre_g_iptt=1|post_enable_tx=1|post_btxok=0|"
                      "post_transmitting=0|post_tune=0|post_tx_when_ready=0|post_g_iptt=0|"
                      "detail=previous_btxok=1"), qPrintable (label + QStringLiteral (" slot-end")));
    ok &= check_log_event (
      log, QStringLiteral ("ptt_off_schedule"),
      QStringLiteral ("pre_enable_tx=1|pre_btxok=0|pre_transmitting=1|pre_tune=0|"
                      "pre_tx_when_ready=0|pre_g_iptt=1|post_enable_tx=1|post_btxok=0|"
                      "post_transmitting=0|post_tune=0|post_tx_when_ready=0|post_g_iptt=0|"
                      "readback_source=legacy_state_or_cached|readback_state=unknown|"
                      "detail=off_delay_ms=200|evidence_scope=software_observation_not_rf_proof|"
                      "off_timer_active=1"), qPrintable (label + QStringLiteral (" schedule")));
    ok &= check_log_event (
      log, QStringLiteral ("ptt_off_dispatch"),
      QStringLiteral ("readback_source=legacy_state_or_cached|readback_state=unknown|"
                      "detail=dispatch=requested|"
                      "evidence_scope=mainwindow_signal_only_not_worker_or_driver|"
                      "off_timer_active=0"), qPrintable (label + QStringLiteral (" dispatch")));
    ok &= check (log.contains (QStringLiteral ("stop_diag event=ptt_off_worker_dispatch "))
                 && log.contains (QStringLiteral ("result_scope=queued_worker_not_driver_result readback=unknown")),
                 "Configuration queued Dummy PTT-OFF to the isolated worker with unknown backend readback");
    std::fprintf (stderr, "DETAIL LOGGER %s real_path=%s off_updates=%d (legacy state/cache; no RF/GET proof)\n",
                  qPrintable (label),
                  qPrintable (MainWindowP0IntegrationTestAccess::rig_control_log_path (window)),
                  counts.ptt_off_updates);
    if (continuous_rx)
      {
        MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, boundary + 15000);
        auto const sends_at_rx_start = counts.modulator_start;
        auto const ticks_at_rx_start = counts.gui_timer_timeouts;
        bool safe_for_entire_rx = true;
        QElapsedTimer wall_rx;
        wall_rx.start ();
        while (MainWindowP0IntegrationTestAccess::virtual_now_ms (window) < boundary + 29900
               && wall_rx.elapsed () < 18000)
          {
            QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
            if (counts.modulator_start != sends_at_rx_start
                || !MainWindowP0IntegrationTestAccess::tx_idle (window)
                || !MainWindowP0IntegrationTestAccess::enable_tx (window)
                || MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
                || MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window)
                || MainWindowP0IntegrationTestAccess::red_tx_residual (window))
              safe_for_entire_rx = false;
            QThread::msleep (2);
          }
        auto const monitored_ticks = counts.gui_timer_timeouts - ticks_at_rx_start;
        std::fprintf (stderr,
                      "DETAIL RX_CONTINUOUS %s elapsed_ms=%lld monitored_gui_ticks=%d sendMessage_delta=%d safe=%d virtual_end_offset_ms=%lld\n",
                      qPrintable (label), static_cast<long long> (wall_rx.elapsed ()), monitored_ticks,
                      counts.modulator_start - sends_at_rx_start, safe_for_entire_rx,
                      static_cast<long long> (MainWindowP0IntegrationTestAccess::virtual_now_ms (window) - boundary));
        ok &= check (safe_for_entire_rx && monitored_ticks >= 130
                     && counts.modulator_start == sends_at_rx_start
                     && MainWindowP0IntegrationTestAccess::virtual_now_ms (window) >= boundary + 29900,
                     "continuous Qt guiTimer observation covers the complete FT8 RX interval without TX residue or a new send");
      }
    else
      {
        for (auto const sample_ms : {15000, 22500, 29900})
          {
            MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, boundary + sample_ms);
            pump_for (125);
            auto const sample_label = label + QStringLiteral (" RX at +%1ms with EnableTX held open").arg (sample_ms);
            ok &= verify_idle_rx (window, counts, true, qPrintable (sample_label));
          }
      }
    return ok;
  }
}

int main (int argc, char ** argv)
{
  if (!make_environment_directories ())
    {
      std::fprintf (stderr, "FAIL: CTest must provide isolated TEMP/TMP/APPDATA/LOCALAPPDATA paths\n");
      return 2;
    }
  bool const stale_only = 2 == argc && 0 == std::strcmp (argv[1], "--stale-only");
  bool const autoseq_retry_cleanup_only = 2 == argc
    && 0 == std::strcmp (argv[1], "--autoseq-retry-cleanup-only");
  if (argc > 1 && !stale_only && !autoseq_retry_cleanup_only)
    {
      std::fprintf (stderr, "FAIL: unknown integration test argument\n");
      return 2;
    }
  int qt_argc = stale_only || autoseq_retry_cleanup_only ? 1 : argc;
  QStandardPaths::setTestModeEnabled (true);
  QApplication app (qt_argc, argv);
  app.setOrganizationName (QStringLiteral ("JTDX-P0-S4.2"));
  app.setApplicationName (QStringLiteral ("JTDX-P0-S4.2-%1").arg (QCoreApplication::applicationPid ()));
  app.setApplicationVersion (version ());
  app.setQuitOnLastWindowClosed (false);
  register_types ();
  qRegisterMetaType<Transceiver::TransceiverState> ("Transceiver::TransceiverState");
  qRegisterMetaType<Transceiver::TransceiverState> ("TransceiverState");

  auto const run_root = QDir {qEnvironmentVariable ("LOCALAPPDATA")}
    .absoluteFilePath (QStringLiteral ("run-%1").arg (QCoreApplication::applicationPid ()));
  if (!QDir {}.mkpath (run_root)) return 2;
  QSettings settings {QDir {run_root}.absoluteFilePath (QStringLiteral ("jtdx.ini")),
                      QSettings::IniFormat};
  settings.beginGroup (QStringLiteral ("Configuration"));
  settings.setValue (QStringLiteral ("Rig"), QStringLiteral ("None"));
  settings.setValue (QStringLiteral ("PTTMethod"),
                     QVariant::fromValue (TransceiverFactory::PTT_method_VOX));
  settings.setValue (QStringLiteral ("MyCall"), QStringLiteral ("K1ABC"));
  settings.setValue (QStringLiteral ("MyGrid"), QStringLiteral ("FN20"));
  settings.setValue (QStringLiteral ("TxDelayInt"), 100);
  for (auto const& key : {QStringLiteral ("CATSerialPort"), QStringLiteral ("CATNetworkPort"),
                          QStringLiteral ("CATUSBPort"), QStringLiteral ("CATTCIPort"),
                          QStringLiteral ("PTTport")}) settings.setValue (key, QString {});
  settings.setValue (QStringLiteral ("CATForceDTR"), false);
  settings.setValue (QStringLiteral ("CATForceRTS"), false);
  settings.setValue (QStringLiteral ("DTR"), false);
  settings.setValue (QStringLiteral ("RTS"), false);
  settings.setValue (QStringLiteral ("SoundInName"), QStringLiteral ("P0-S4.2-no-input-device"));
  settings.setValue (QStringLiteral ("SoundOutName"), QStringLiteral ("P0-S4.2-no-output-device"));
  settings.setValue (QStringLiteral ("PSKReporter"), false);
  settings.setValue (QStringLiteral ("AllowSpotsDXSummit"), false);
  settings.setValue (QStringLiteral ("AcceptUDPRequests"), false);
  settings.setValue (QStringLiteral ("EnableUDP1adifSending"), false);
  settings.setValue (QStringLiteral ("EnableUDP2adifBroadcast"), false);
  settings.setValue (QStringLiteral ("EnableTCPConnection"), false);
  settings.setValue (QStringLiteral ("UDPServer"), QStringLiteral ("127.0.0.1"));
  settings.setValue (QStringLiteral ("UDP2Server"), QStringLiteral ("127.0.0.1"));
  settings.setValue (QStringLiteral ("TCPServer"), QStringLiteral ("127.0.0.1"));
  settings.setValue (QStringLiteral ("WebUiEnabled"), false);
  settings.setValue (QStringLiteral ("WebUiAutomaticPort"), true);
  settings.setValue (QStringLiteral ("WebUiBindAddress"), QStringLiteral ("127.0.0.1"));
  if (autoseq_retry_cleanup_only) {
    settings.setValue (QStringLiteral ("SeqAnswerCQCount"), true);
    settings.setValue (QStringLiteral ("SeqAnswerCQCounterValue"), 2);
    settings.setValue (QStringLiteral ("SeqAutoCallNewDXCC"), true);
    settings.setValue (QStringLiteral ("SeqAutoCallNewDXCCBandMode"), false);
    settings.setValue (QStringLiteral ("SeqAutoCallNewGrid"), false);
    settings.setValue (QStringLiteral ("SeqAutoCallNewGridBandMode"), false);
    settings.setValue (QStringLiteral ("SeqAutoCallNewCall"), false);
    settings.setValue (QStringLiteral ("SeqAutoCallNewCallBand"), false);
    settings.setValue (QStringLiteral ("SeqAutoAnswerDirectedCalls"), false);
  }
  settings.endGroup ();
  settings.beginGroup (QStringLiteral ("MainWindow"));
  settings.setValue (QStringLiteral ("Mode"), QStringLiteral ("FT8"));
  settings.setValue (QStringLiteral ("ModeTx"), QStringLiteral ("FT8"));
  settings.setValue (QStringLiteral ("TxFirst"), true);
  settings.endGroup ();
  settings.sync ();
  if (!check (QSettings::NoError == settings.status (), "isolated QSettings file is writable")) return 2;

  QSharedMemory shared_memory {app.applicationName () + QStringLiteral ("-shared")};
  if (!stale_only)
    {
      if (!check (shared_memory.create (sizeof (struct dec_data)), "isolated shared-memory segment created")) return 2;
      std::memset (shared_memory.data (), 0, static_cast<std::size_t> (shared_memory.size ()));
    }
  QNetworkAccessManager network_manager {&app};
  int failures = 0;
  bool window_closed = false;
  if (!stale_only && !autoseq_retry_cleanup_only)
    {
  {
    MainWindow window {true, &settings, &shared_memory, 4, &network_manager,
                       QProcessEnvironment::systemEnvironment ()};
    MainWindowP0IntegrationTestAccess::SignalCounts observed_signals;
    MainWindowP0IntegrationTestAccess::observe_signals (window, observed_signals);
    MainWindowP0IntegrationTestAccess::observe_finished (window, window_closed);
    std::fprintf (stderr, "DETAIL VERSION app=%s data_dir=%s temp=%s local_appdata=%s allowed_root=%s\n",
                  qPrintable (app.applicationVersion ()),
                  qPrintable (MainWindowP0IntegrationTestAccess::data_directory (window)),
                  qPrintable (qEnvironmentVariable ("TEMP")),
                  qPrintable (qEnvironmentVariable ("LOCALAPPDATA")), P0_TX_MAINWINDOW_TEST_ALLOWED_ROOT);
    MainWindowP0IntegrationTestAccess::print_state (window, "constructed");
    failures += !check (MainWindowP0IntegrationTestAccess::safe_configuration (window)
                        && !MainWindowP0IntegrationTestAccess::transceiver_online (window),
                        "MainWindow starts offline with services disabled and no decoder process");
    failures += !check (MainWindowP0IntegrationTestAccess::configuration_has_safe_endpoints (window),
                        "Dummy VOX session has Rig=None, empty serial/network/PTT endpoints and DTR/RTS disabled");
    auto * const gui_timer = MainWindowP0IntegrationTestAccess::gui_timer (window);
    failures += !check (gui_timer->isActive () && 100 == gui_timer->interval (),
                        "real MainWindow guiTimer is active at its production 100ms interval");
    window.show ();
    pump_for (350);
    failures += !check (observed_signals.gui_timer_timeouts >= 2,
                        "real guiUpdate runs repeatedly in the Qt event loop");
    failures += !check (0 == observed_signals.audio_input_start
                        && 0 == observed_signals.audio_output_initialize
                        && 0 == observed_signals.modulator_start,
                        "isolated constructor emits no real audio or modulator-start signal");

    MainWindowP0IntegrationTestAccess::activate_ft8 (window);
    MainWindowP0IntegrationTestAccess::rig_open (window);
    bool const online = wait_until ([&] {
      return MainWindowP0IntegrationTestAccess::transceiver_online (window)
        && MainWindowP0IntegrationTestAccess::rig_state_online (window)
        && MainWindowP0IntegrationTestAccess::generation (window)
             == MainWindowP0IntegrationTestAccess::state_generation (window);
    }, 5000);
    std::fprintf (stderr, "DETAIL DUMMY_RIG online=%d generation=%llu state_generation=%llu updates=%d monitoring=%d\n",
                  online, static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::generation (window)),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::state_generation (window)),
                  observed_signals.config_updates, MainWindowP0IntegrationTestAccess::monitoring (window));
    failures += !check (online && MainWindowP0IntegrationTestAccess::safe_configuration (window),
                        "real Configuration::rigOpen opens the isolated Dummy session only");
    failures += !check (MainWindowP0IntegrationTestAccess::tx_idle (window), "Dummy session starts TX-idle");

    for (int cycle = 1; cycle <= 2; ++cycle)
      failures += !run_ft8_cycle (window, observed_signals, true, cycle, 1 == cycle);
    for (int cycle = 1; cycle <= 2; ++cycle)
      failures += !run_ft8_cycle (window, observed_signals, false, cycle, 1 == cycle);

    MainWindowP0IntegrationTestAccess::set_tx_first_via_button (window, true);
    MainWindowP0IntegrationTestAccess::enable_tx_via_button (window, true);
    auto const manual_log_offset = QFileInfo {
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
    auto const manual_boundary = next_slot_boundary (
      MainWindowP0IntegrationTestAccess::virtual_now_ms (window), true);
    MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, manual_boundary + 1500);
    auto const manual_sends_before = observed_signals.modulator_start;
    bool const manual_started = wait_until ([&] {
      return observed_signals.modulator_start > manual_sends_before
        && MainWindowP0IntegrationTestAccess::tx_active (window);
    }, 7000);
    failures += !check (manual_started, "manual-stop case reaches real scheduled FT8 TX");
    if (manual_started)
      {
        MainWindowP0IntegrationTestAccess::manual_stop_via_button (window);
        auto const manual_timer_before = observed_signals.ptt_off_timer_timeouts;
        bool const manual_stopped = wait_until ([&] {
          return MainWindowP0IntegrationTestAccess::tx_idle (window)
            && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window);
        }, 1500);
        bool const manual_off = wait_until ([&] {
          return observed_signals.ptt_off_timer_timeouts > manual_timer_before
            && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
            && !MainWindowP0IntegrationTestAccess::rig_state_ptt (window);
        }, 2500);
        std::fprintf (stderr, "DETAIL MANUAL_STOP started=%d stopped=%d off=%d enabled_after=%d timer_timeouts=%d\n",
                      manual_started, manual_stopped, manual_off,
                      MainWindowP0IntegrationTestAccess::enable_tx (window),
                      observed_signals.ptt_off_timer_timeouts);
        failures += !check (manual_stopped && manual_off
                            && !MainWindowP0IntegrationTestAccess::enable_tx (window),
                            "manual UI Stop Tx disables EnableTX and dispatches real stopTx2 after 200ms");
        pump_for (100);
        auto const log = read_log_from (
          MainWindowP0IntegrationTestAccess::rig_control_log_path (window), manual_log_offset);
        failures += !check_log_event (
          log, QStringLiteral ("ptt_off_schedule"),
          QStringLiteral ("post_enable_tx=0|post_btxok=0|post_transmitting=0|post_g_iptt=0|"
                          "readback_source=legacy_state_or_cached|readback_state=unknown|"
                          "detail=off_delay_ms=200|evidence_scope=software_observation_not_rf_proof"),
          "manual stop schedule");
      }

    auto const tune_log_offset = QFileInfo {
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
    MainWindowP0IntegrationTestAccess::tune_via_button (window);
    auto const tune_sends_before = observed_signals.modulator_start;
    bool const tune_started = wait_until ([&] {
      return observed_signals.modulator_start > tune_sends_before
        && MainWindowP0IntegrationTestAccess::tune_active (window);
    }, 7000);
    failures += !check (tune_started, "Tune reaches MainWindow PTT and sendMessage chain");
    if (tune_started)
      {
        MainWindowP0IntegrationTestAccess::tune_via_button (window);
        bool const tune_stop_timer = wait_until ([&] {
          return MainWindowP0IntegrationTestAccess::tune_timer_active (window);
        }, 500);
        auto const tune_off_before = observed_signals.ptt_off_timer_timeouts;
        bool const tune_stopped = wait_until ([&] {
          return MainWindowP0IntegrationTestAccess::tx_idle (window)
            && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window);
        }, 2500);
        bool const tune_off = wait_until ([&] {
          return observed_signals.ptt_off_timer_timeouts > tune_off_before
            && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
            && !MainWindowP0IntegrationTestAccess::rig_state_ptt (window);
        }, 2500);
        std::fprintf (stderr, "DETAIL TUNE_STOP started=%d halt_timer=%d stopped=%d off=%d\n",
                      tune_started, tune_stop_timer, tune_stopped, tune_off);
        failures += !check (tune_stop_timer && tune_stopped && tune_off,
                            "Tune-off traverses actual halt timer, stopTx and 200ms stopTx2");
        pump_for (100);
        auto const tune_log = read_log_from (
          MainWindowP0IntegrationTestAccess::rig_control_log_path (window), tune_log_offset);
        failures += !check_log_event (
          tune_log, QStringLiteral ("ptt_off_schedule"),
          QStringLiteral ("detail=off_delay_ms=200|readback_source=legacy_state_or_cached|"
                          "readback_state=unknown|evidence_scope=software_observation_not_rf_proof"),
          "Tune stop schedule");
        failures += !check_log_event (
          tune_log, QStringLiteral ("ptt_off_dispatch"),
          QStringLiteral ("detail=dispatch=requested|"
                          "evidence_scope=mainwindow_signal_only_not_worker_or_driver"),
          "Tune stop dispatch");
      }

    // A same-generation software OFF comes through the real Configuration slot
    // and Dummy worker command path while a genuine FT8 TX is active.
    auto const early_off_log_offset = QFileInfo {
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
    MainWindowP0IntegrationTestAccess::set_tx_first_via_button (window, true);
    MainWindowP0IntegrationTestAccess::enable_tx_via_button (window, true);
    auto const early_boundary = next_slot_boundary (
      MainWindowP0IntegrationTestAccess::virtual_now_ms (window), true);
    MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, early_boundary + 1500);
    auto const early_sends_before = observed_signals.modulator_start;
    bool const early_tx_started = wait_until ([&] {
      return observed_signals.modulator_start > early_sends_before
        && MainWindowP0IntegrationTestAccess::tx_active (window);
    }, 7000);
    auto const early_generation = MainWindowP0IntegrationTestAccess::generation (window);
    auto const early_token = MainWindowP0IntegrationTestAccess::token_serial (window);
    auto const early_off_timer_before = observed_signals.ptt_off_timer_timeouts;
    failures += !check (early_tx_started, "same-generation early-OFF scenario starts real FT8 TX");
    if (early_tx_started)
      {
        MainWindowP0IntegrationTestAccess::request_config_ptt_off (window);
        bool const early_stopped = wait_until ([&] {
          return MainWindowP0IntegrationTestAccess::tx_idle (window)
            && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window);
        }, 2000);
        bool const early_off_dispatched = wait_until ([&] {
          return observed_signals.ptt_off_timer_timeouts > early_off_timer_before
            && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
            && !MainWindowP0IntegrationTestAccess::rig_state_ptt (window);
        }, 2500);
        auto const sends_before_old_callback = observed_signals.modulator_start;
        bool const old_callback_queued = QMetaObject::invokeMethod (
          &window, "startTx2", Qt::QueuedConnection);
        pump_for (250);
        bool const no_revival = observed_signals.modulator_start == sends_before_old_callback
          && MainWindowP0IntegrationTestAccess::tx_idle (window)
          && MainWindowP0IntegrationTestAccess::generation (window) == early_generation;
        std::fprintf (stderr,
                      "DETAIL SAME_GEN_EARLY_OFF started=%d stopped=%d off_dispatched=%d generation=%llu token=%llu callback_queued=%d no_revival=%d off_updates=%d\n",
                      early_tx_started, early_stopped, early_off_dispatched,
                      static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::generation (window)),
                      static_cast<unsigned long long> (early_token), old_callback_queued, no_revival,
                      observed_signals.ptt_off_updates);
        failures += !check (early_stopped && early_off_dispatched && old_callback_queued && no_revival
                            && MainWindowP0IntegrationTestAccess::state_generation (window) == early_generation,
                            "current-generation OFF interrupts real TX and its old timer callback cannot revive it");
        pump_for (100);
        auto const early_log = read_log_from (
          MainWindowP0IntegrationTestAccess::rig_control_log_path (window), early_off_log_offset);
        failures += !check_log_event (
          early_log, QStringLiteral ("ptt_off_schedule"),
          QStringLiteral ("readback_source=legacy_state_or_cached|readback_state=unknown|"
                          "detail=off_delay_ms=200|evidence_scope=software_observation_not_rf_proof"),
          "same-generation early OFF schedule");
        failures += !check (early_log.contains (QStringLiteral ("stop_diag event=ptt_off_worker_dispatch "))
                            && early_log.contains (QStringLiteral ("result_scope=queued_worker_not_driver_result readback=unknown")),
                            "same-generation early OFF reached the real Dummy worker queue; no GET/RF evidence claimed");
      }

    MainWindowP0IntegrationTestAccess::rig_open (window);
    bool const reopened = wait_until ([&] {
      return MainWindowP0IntegrationTestAccess::transceiver_online (window)
        && MainWindowP0IntegrationTestAccess::rig_state_online (window)
        && MainWindowP0IntegrationTestAccess::generation (window)
             == MainWindowP0IntegrationTestAccess::state_generation (window);
    }, 5000);
    failures += !check (reopened, "active Dummy session remains current for Configuration slot classification");
    auto const current_generation = MainWindowP0IntegrationTestAccess::generation (window);
    auto const updates_before = observed_signals.config_updates;
    auto const ptt_on_updates_before = observed_signals.ptt_on_updates;
    auto const state_generation_before = MainWindowP0IntegrationTestAccess::state_generation (window);
    auto const stale_config_log_offset = QFileInfo {
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
    QString stale_signature;
    bool const stale_queued = MainWindowP0IntegrationTestAccess::queue_stale_configuration_update (
      window, &stale_signature);
    pump_for (250);
    auto const stale_log = read_log_from (MainWindowP0IntegrationTestAccess::rig_control_log_path (window), stale_config_log_offset);
    auto const stale_filtered = stale_log.contains (
      QStringLiteral ("event=update_filtered generation=%1 current_generation=%2")
        .arg (current_generation - 1).arg (current_generation))
      && stale_log.contains (QStringLiteral ("filter_cause=stale_generation"));
    std::fprintf (stderr, "DETAIL STALE_CONFIG queued=%d signature='%s' generation=%llu state_gen=%llu updates_before=%d updates_after=%d ptt_on_before=%d ptt_on_after=%d filtered=%d\n",
                  stale_queued, qPrintable (stale_signature),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::generation (window)),
                  static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::state_generation (window)),
                  updates_before, observed_signals.config_updates, ptt_on_updates_before,
                  observed_signals.ptt_on_updates, stale_filtered);
    failures += !check (stale_queued && stale_filtered
                        && current_generation == MainWindowP0IntegrationTestAccess::generation (window)
                        && state_generation_before == MainWindowP0IntegrationTestAccess::state_generation (window)
                        && ptt_on_updates_before == observed_signals.ptt_on_updates
                        && !MainWindowP0IntegrationTestAccess::rig_state_ptt (window),
                        "real Configuration::impl slot filters injected stale-generation state without forwarding");

    failures += !check (0 == observed_signals.audio_input_start
                        && 0 == observed_signals.audio_output_initialize,
                        "all Dummy TX/Tune/stop paths emit no audio initialize/start signal");
    std::fprintf (stderr, "DETAIL COUNTS gui_ticks=%d updates=%d on_updates=%d off_updates=%d ptt0_timeouts=%d sendMessage=%d audio_in=%d audio_out=%d\n",
                  observed_signals.gui_timer_timeouts, observed_signals.config_updates,
                  observed_signals.ptt_on_updates, observed_signals.ptt_off_updates,
                  observed_signals.ptt_off_timer_timeouts,
                  observed_signals.modulator_start, observed_signals.audio_input_start,
                  observed_signals.audio_output_initialize);
    auto const shutdown_log_offset = QFileInfo {
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
    MainWindowP0IntegrationTestAccess::enable_tx_via_button (window, true);
    auto const shutdown_boundary = next_slot_boundary (
      MainWindowP0IntegrationTestAccess::virtual_now_ms (window), true);
    MainWindowP0IntegrationTestAccess::set_virtual_time_ms (window, shutdown_boundary + 1500);
    auto const shutdown_sends_before = observed_signals.modulator_start;
    bool const closing_tx_started = wait_until ([&] {
      return observed_signals.modulator_start > shutdown_sends_before
        && MainWindowP0IntegrationTestAccess::tx_active (window);
    }, 7000);
    auto const shutdown_generation_before = MainWindowP0IntegrationTestAccess::generation (window);
    failures += !check (closing_tx_started,
                        "shutdown case begins from an active online MainWindow TX");
    std::fprintf (stderr, "DETAIL CLOSE_ACTIVE_TX started=%d generation=%llu tx=%d ptt=%d\n",
                  closing_tx_started,
                  static_cast<unsigned long long> (shutdown_generation_before),
                  MainWindowP0IntegrationTestAccess::transmitting (window),
                  MainWindowP0IntegrationTestAccess::rig_state_ptt (window));
    auto const sends_at_close = observed_signals.modulator_start;
    window.close ();
    pump_for (300);
    bool const shutdown_logged = wait_until ([&] {
      auto const tail = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), shutdown_log_offset);
      return tail.contains (QStringLiteral ("stop_diag event=shutdown_ptt_off "))
        && tail.contains (QStringLiteral ("stop_diag event=shutdown_backend_stop "));
    }, 5000);
    auto const shutdown_log = read_log_from (
      MainWindowP0IntegrationTestAccess::rig_control_log_path (window), shutdown_log_offset);
    std::fprintf (stderr, "DETAIL SHUTDOWN_LOG captured=%d active_tx=%d %s\n",
                  shutdown_logged, closing_tx_started, qPrintable (shutdown_log));
    failures += !check (window_closed && !MainWindowP0IntegrationTestAccess::gui_timer (window)->isActive ()
                        && !MainWindowP0IntegrationTestAccess::transceiver_online (window)
                        && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window)
                        && QProcess::NotRunning == MainWindowP0IntegrationTestAccess::decoder_state (window)
                        && observed_signals.modulator_start == sends_at_close,
                        "closeEvent on active TX stops MainWindow timers, takes Dummy session offline, and prevents another send");
    failures += !check (closing_tx_started && shutdown_logged
                        && shutdown_log.contains (QStringLiteral ("event=close"))
                        && shutdown_log.contains (QStringLiteral ("reason=requested_offline"))
                        && shutdown_log.contains (QStringLiteral ("callsite=transceiver_offline")),
                        "online active-TX close enters Configuration worker shutdown and records close reason");
    failures += !check_log_event (
      shutdown_log, QStringLiteral ("shutdown_ptt_off"),
      QStringLiteral ("requested_online=1|result_scope=driver_method_only|readback=unknown"),
      "active-TX shutdown PTT-off");
    failures += !check_log_event (
      shutdown_log, QStringLiteral ("shutdown_backend_stop"),
      QStringLiteral ("outcome=returned|reason=backend_stop_and_post_stop_returned|"
                      "result_scope=driver_method_only|readback=unknown"),
      "active-TX shutdown backend stop");
    std::fprintf (stderr, "DETAIL SHUTDOWN finished=%d gui_timer=%d online=%d ptt1=%d decoder=%d sends=%d\n",
                  window_closed, MainWindowP0IntegrationTestAccess::gui_timer (window)->isActive (),
                  MainWindowP0IntegrationTestAccess::transceiver_online (window),
                  MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window),
                  MainWindowP0IntegrationTestAccess::decoder_state (window),
                  observed_signals.modulator_start);
  }
    }

  if (autoseq_retry_cleanup_only)
    {
      MainWindow window {true, &settings, &shared_memory, 4, &network_manager,
                         QProcessEnvironment::systemEnvironment ()};
      MainWindowP0IntegrationTestAccess::SignalCounts observed_signals;
      MainWindowP0IntegrationTestAccess::observe_signals (window, observed_signals);
      failures += !check (MainWindowP0IntegrationTestAccess::safe_configuration (window)
                          && MainWindowP0IntegrationTestAccess::configuration_has_safe_endpoints (window),
                          "AutoSeq retry fixture uses isolated Rig=None/VOX with no external endpoints or decoder");
      window.show ();
      auto const receive_time = 40000u;
      auto const first_decode_started = QDateTime::currentMSecsSinceEpoch ();
      for (bool const single_shot : {false, true})
        for (bool const skip_tx1 : {false, true})
          {
            MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
                window, single_shot, skip_tx1, false);
            MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
                window, first_decode_started);
            MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
                window, QStringLiteral ("K2OLD"), 2, receive_time);
            auto uncooled_history = MainWindowP0IntegrationTestAccess::auto_seq_history_copy (window);
            bool const retryTargetWouldOtherwiseBeEligible =
              MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
                  uncooled_history, QStringLiteral ("K2OLD"), receive_time);
            MainWindowP0IntegrationTestAccess::prime_pending_tx_request (window);
            failures += !check (
                MainWindowP0IntegrationTestAccess::retry_threshold_preconditions (window, 2, 2),
                "retry threshold fixture uses raw/effective RFIN retry counts 2->2 below the steady-clock timeout");
            auto const log_offset = QFileInfo {
              MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
            auto const sends_before = observed_signals.modulator_start;
            MainWindowP0IntegrationTestAccess::process_auto (window);
            auto const cleanup_log = read_log_from (
              MainWindowP0IntegrationTestAccess::rig_control_log_path (window), log_offset);
            bool const one_halt = cleanup_log.count (QStringLiteral ("event=halt_tx_requested")) == 1;
            bool const cleanup_state = MainWindowP0IntegrationTestAccess::current_dx_call (window).isEmpty ()
              && !MainWindowP0IntegrationTestAccess::operator_hold (window)
              && !MainWindowP0IntegrationTestAccess::stall_owner_active (window)
              && MainWindowP0IntegrationTestAccess::suppressed_call (window).isEmpty ()
              && MainWindowP0IntegrationTestAccess::retry_states_clear (window)
              && MainWindowP0IntegrationTestAccess::batch_skipped (window);
            failures += !check (one_halt && cleanup_state,
                                "real retry threshold halts once, cancels its owner, revokes pending TX and skips the batch without artificial hold");
            failures += !check (cleanup_log.contains (QStringLiteral (
                                  "event=standby_cleanup_complete"))
                                && cleanup_log.contains (QStringLiteral (
                                  "cleanup_target_before=K2OLD"))
                                && cleanup_log.contains (QStringLiteral (
                                  "cleanup_target_after="))
                                && cleanup_log.contains (QStringLiteral (
                                  "operator_hold_before=false"))
                                && cleanup_log.contains (QStringLiteral (
                                  "operator_hold_after=false"))
                                && cleanup_log.contains (QStringLiteral (
                                  "owner_active_before=true"))
                                && cleanup_log.contains (QStringLiteral (
                                  "owner_active_after=false")),
                                "cleanup diagnostic records target, hold and owner before/after values");
            failures += !check (observed_signals.modulator_start == sends_before
                                && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window)
                                && !MainWindowP0IntegrationTestAccess::ptt_token_active (window),
                                "revoked pending request cannot restart audio/PTT in the RX path");

            MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
                window, first_decode_started + 15000);
            auto const retryCandidateTime = receive_time + 15;
            MainWindowP0IntegrationTestAccess::seed_auto_candidate (
                window, QStringLiteral ("K2OLD"), retryCandidateTime);
            MainWindowP0IntegrationTestAccess::seed_auto_candidate (
                uncooled_history, QStringLiteral ("K2OLD"), retryCandidateTime);
            bool const sameTargetWouldBeEligibleWithoutCooldown =
              MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
                  uncooled_history, QStringLiteral ("K2OLD"), retryCandidateTime);
            MainWindowP0IntegrationTestAccess::process_auto (window);
            failures += !check (MainWindowP0IntegrationTestAccess::current_dx_call (window).isEmpty ()
                                && !MainWindowP0IntegrationTestAccess::enable_tx (window)
                                && !MainWindowP0IntegrationTestAccess::stall_owner_active (window)
                                && retryTargetWouldOtherwiseBeEligible
                                && sameTargetWouldBeEligibleWithoutCooldown,
                                "fresh same-target RFIN at the same report and within 5 minutes is independently eligible but cooldown blocks selection/arm");

            MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
                window, first_decode_started + 30000);
            MainWindowP0IntegrationTestAccess::seed_auto_candidate (
                window, QStringLiteral ("W1NEW"), receive_time + 30);
            MainWindowP0IntegrationTestAccess::process_auto (window);
            failures += !check (MainWindowP0IntegrationTestAccess::current_dx_call (window)
                                  == QStringLiteral ("W1NEW")
                                && MainWindowP0IntegrationTestAccess::enable_tx (window)
                                && MainWindowP0IntegrationTestAccess::stall_owner_active (window)
                                && MainWindowP0IntegrationTestAccess::stall_owner_call (window)
                                     == QStringLiteral ("W1NEW"),
                                "next fresh batch selects and arms a different eligible target through real MainWindow gates");
            failures += !check (observed_signals.modulator_start == sends_before
                                && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window)
                                && 0 == g_iptt,
                                "candidate EnableTX arm does not start PTT or modulator in the RX test path");
          }

      // The pending-token cases above prove request revocation. This separate
      // software-active state makes the old SingleShot fallback see g_iptt=1,
      // so the before-image issues a second halt. No rig is opened and the
      // monitor remains disabled; RX cleanup still uses the real MainWindow
      // guiUpdate()/stopTx()/200 ms stopTx2 path.
      MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
          window, true, false, false);
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 37500);
      MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
          window, QStringLiteral ("K2ACTIVE"), 3, receive_time);
      MainWindowP0IntegrationTestAccess::prime_active_software_tx (window);
      failures += !check (
          MainWindowP0IntegrationTestAccess::retry_threshold_preconditions (window, 3, 2),
          "software-active fixture accounts for in-flight TX with raw/effective retry counts 3->2 below timeout");
      auto const active_log_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
      auto const active_sends_before = observed_signals.modulator_start;
      auto const active_audio_before = observed_signals.audio_input_start;
      auto const active_off_timeouts_before = observed_signals.ptt_off_timer_timeouts;
      MainWindowP0IntegrationTestAccess::process_auto (window);
      auto const active_cleanup_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), active_log_offset);
      failures += !check (active_cleanup_log.count (QStringLiteral ("event=halt_tx_requested")) == 1
                          && !MainWindowP0IntegrationTestAccess::operator_hold (window)
                          && !MainWindowP0IntegrationTestAccess::stall_owner_active (window)
                          && !MainWindowP0IntegrationTestAccess::transmitting (window)
                          && !MainWindowP0IntegrationTestAccess::btxok (window)
                          && 1 == g_iptt,
                          "software-active threshold cleanup emits one halt while leaving pending RX PTT for GUI stop");
      MainWindowP0IntegrationTestAccess::request_rx_stop_for_test (window);
      MainWindowP0IntegrationTestAccess::drive_gui_update (window);
      bool const rxStopScheduled = MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window)
        && 200 == MainWindowP0IntegrationTestAccess::ptt_off_timer_interval (window)
        && 0 == g_iptt
        && !MainWindowP0IntegrationTestAccess::transmitting (window)
        && !MainWindowP0IntegrationTestAccess::btxok (window);
      bool const rxOffTimerCompleted = wait_until ([&] {
        return observed_signals.ptt_off_timer_timeouts > active_off_timeouts_before
          && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (window);
      }, 1500);
      auto const active_final_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), active_log_offset);
      failures += !check (rxStopScheduled && rxOffTimerCompleted
                          && active_final_log.contains (QStringLiteral ("event=ptt_off_schedule"))
                          && active_final_log.count (QStringLiteral ("event=halt_tx_requested")) == 1,
                          "real guiUpdate schedules one RX stop and the 200 ms OFF timer completes without another halt");
      failures += !check (observed_signals.modulator_start == active_sends_before
                          && observed_signals.audio_input_start == active_audio_before
                          && 0 == g_iptt
                          && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (window),
                          "software-active regression fixture never starts audio/modulator or reasserts PTT");

      MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
          window, false, false, true);
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 45000);
      MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
          window, QStringLiteral ("K2HELD"), 2, receive_time);
      failures += !check (
          MainWindowP0IntegrationTestAccess::retry_threshold_preconditions (window, 2, 2),
          "pre-held threshold fixture uses raw/effective retry counts 2->2 below timeout");
      auto const held_cleanup_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
      MainWindowP0IntegrationTestAccess::prime_pending_tx_request (window);
      MainWindowP0IntegrationTestAccess::process_auto (window);
      auto const held_cleanup_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), held_cleanup_offset);
      failures += !check (MainWindowP0IntegrationTestAccess::operator_hold (window)
                          && !MainWindowP0IntegrationTestAccess::stall_owner_active (window)
                          && MainWindowP0IntegrationTestAccess::retry_states_clear (window)
                          && held_cleanup_log.count (QStringLiteral ("event=halt_tx_requested")) == 1,
                          "threshold cleanup preserves an already-set operator hold while ending its owner once");
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 60000);
      MainWindowP0IntegrationTestAccess::seed_auto_candidate (
          window, QStringLiteral ("W1HELD"), receive_time + 15);
      auto held_candidate_uncooled_history =
        MainWindowP0IntegrationTestAccess::auto_seq_history_copy (window);
      bool const heldCandidateWouldOtherwiseBeEligible =
        MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
            held_candidate_uncooled_history, QStringLiteral ("W1HELD"), receive_time + 15);
      auto const held_candidate_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
      MainWindowP0IntegrationTestAccess::process_auto (window);
      auto const held_candidate_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), held_candidate_offset);
      failures += !check (MainWindowP0IntegrationTestAccess::current_dx_call (window)
                            == QStringLiteral ("W1HELD")
                          && !MainWindowP0IntegrationTestAccess::enable_tx (window)
                          && !MainWindowP0IntegrationTestAccess::stall_owner_active (window)
                          && held_candidate_log.contains (QStringLiteral (
                            "event=automatic_candidate_withheld"))
                          && held_candidate_log.contains (QStringLiteral (
                            "reason=operator_hold")),
                          "fresh priority-22 held candidate is otherwise eligible, remains visible, and is withheld before automatic EnableTX arm");
      failures += !check (heldCandidateWouldOtherwiseBeEligible,
                          "hold test candidate passes the uncooled real QsoHistory selector");

      MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
          window, false, false, false);
      MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
          window, QStringLiteral ("K2STOP"), 1, receive_time);
      MainWindowP0IntegrationTestAccess::prime_manual_enable_tx (window);
      MainWindowP0IntegrationTestAccess::manual_stop_via_button (window);
      bool const manualStopHeld = MainWindowP0IntegrationTestAccess::operator_hold (window)
        && !MainWindowP0IntegrationTestAccess::stall_owner_active (window);
      MainWindowP0IntegrationTestAccess::operator_clear_dx (window);
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 75000);
      MainWindowP0IntegrationTestAccess::seed_auto_candidate (
          window, QStringLiteral ("W1STOP"), receive_time + 15);
      auto stop_candidate_history =
        MainWindowP0IntegrationTestAccess::auto_seq_history_copy (window);
      bool const stopCandidateWouldOtherwiseBeEligible =
        MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
            stop_candidate_history, QStringLiteral ("W1STOP"), receive_time + 15);
      MainWindowP0IntegrationTestAccess::process_auto (window);
      failures += !check (manualStopHeld
                          && MainWindowP0IntegrationTestAccess::operator_hold (window)
                          && !MainWindowP0IntegrationTestAccess::enable_tx (window)
                          && stopCandidateWouldOtherwiseBeEligible,
                          "manual Stop Tx remains a hold that blocks a different fresh candidate");

      MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
          window, false, false, false);
      MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
          window, QStringLiteral ("K2CLEAR"), 1, receive_time);
      auto const clear_log_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
      MainWindowP0IntegrationTestAccess::operator_clear_dx (window);
      auto const clear_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window), clear_log_offset);
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 90000);
      MainWindowP0IntegrationTestAccess::seed_auto_candidate (
          window, QStringLiteral ("W1CLEAR"), receive_time + 15);
      auto clear_candidate_history =
        MainWindowP0IntegrationTestAccess::auto_seq_history_copy (window);
      bool const clearCandidateWouldOtherwiseBeEligible =
        MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
            clear_candidate_history, QStringLiteral ("W1CLEAR"), receive_time + 15);
      auto const clear_candidate_log_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window)}.size ();
      MainWindowP0IntegrationTestAccess::process_auto (window);
      auto const clear_candidate_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (window),
        clear_candidate_log_offset);
      failures += !check (MainWindowP0IntegrationTestAccess::operator_hold (window)
                          && MainWindowP0IntegrationTestAccess::current_dx_call (window)
                               == QStringLiteral ("W1CLEAR")
                          && !MainWindowP0IntegrationTestAccess::enable_tx (window)
                          && clear_log.contains (QStringLiteral ("event=operator_hold_set"))
                          && clear_log.contains (QStringLiteral ("reason=manual_dx_clear"))
                          && clear_candidate_log.contains (QStringLiteral (
                            "event=automatic_candidate_withheld"))
                          && clear_candidate_log.contains (QStringLiteral (
                            "reason=operator_hold"))
                          && clear_candidate_log.contains (QStringLiteral ("call=W1CLEAR"))
                          && clearCandidateWouldOtherwiseBeEligible,
                          "manual Clear DX records its source and blocks a different fresh candidate");

      MainWindowP0IntegrationTestAccess::reset_auto_seq_retry_fixture (
          window, false, false, false);
      MainWindowP0IntegrationTestAccess::seed_auto_retry_target (
          window, QStringLiteral ("K2OFF"), 1, receive_time);
      MainWindowP0IntegrationTestAccess::operator_disable_auto_seq (window);
      bool const autoSeqOffHeld = !MainWindowP0IntegrationTestAccess::auto_sequence_enabled (window)
        && MainWindowP0IntegrationTestAccess::operator_hold (window)
        && !MainWindowP0IntegrationTestAccess::stall_owner_active (window);
      MainWindowP0IntegrationTestAccess::operator_clear_dx (window);
      MainWindowP0IntegrationTestAccess::begin_fresh_auto_seq_batch (
          window, first_decode_started + 105000);
      MainWindowP0IntegrationTestAccess::seed_auto_candidate (
          window, QStringLiteral ("W1OFF"), receive_time + 15);
      auto off_candidate_history =
        MainWindowP0IntegrationTestAccess::auto_seq_history_copy (window);
      bool const offCandidateWouldOtherwiseBeEligible =
        MainWindowP0IntegrationTestAccess::auto_seq_selects_rare_candidate (
            off_candidate_history, QStringLiteral ("W1OFF"), receive_time + 15);
      MainWindowP0IntegrationTestAccess::process_auto (window);
      failures += !check (autoSeqOffHeld
                          && !MainWindowP0IntegrationTestAccess::auto_sequence_enabled (window)
                          && !MainWindowP0IntegrationTestAccess::enable_tx (window)
                          && offCandidateWouldOtherwiseBeEligible,
                          "AutoSeq-off remains disabled and cannot arm a candidate");

      failures += !check (0 == observed_signals.audio_input_start
                          && 0 == observed_signals.audio_output_initialize
                          && 0 == observed_signals.modulator_start
                          && QProcess::NotRunning == MainWindowP0IntegrationTestAccess::decoder_state (window),
                          "retry integration fixture starts no audio, modulator or decoder process");
      window.close ();
    }

  // Exercise the stale queued start through a real MainWindow::rigOpen()
  // configuration-parameter reopen on a separate isolated Dummy instance.
  if (stale_only)
    {
  settings.beginGroup (QStringLiteral ("Configuration"));
  settings.setValue (QStringLiteral ("TxDelayInt"), 500);
  settings.endGroup ();
  settings.sync ();
  QSharedMemory stale_shared_memory {app.applicationName () + QStringLiteral ("-stale-shared")};
  failures += !check (stale_shared_memory.create (sizeof (struct dec_data)),
                      "separate stale-reopen shared-memory segment created");
  if (stale_shared_memory.isAttached ())
    std::memset (stale_shared_memory.data (), 0,
                 static_cast<std::size_t> (stale_shared_memory.size ()));
  if (stale_shared_memory.isAttached ())
    {
      MainWindow stale_window {true, &settings, &stale_shared_memory, 4, &network_manager,
                               QProcessEnvironment::systemEnvironment ()};
      MainWindowP0IntegrationTestAccess::SignalCounts stale_signals;
      MainWindowP0IntegrationTestAccess::observe_signals (stale_window, stale_signals);
      stale_window.show ();
      pump_for (250);
      MainWindowP0IntegrationTestAccess::print_state (stale_window, "stale_constructed");
      MainWindowP0IntegrationTestAccess::print_endpoint_state (stale_window);
      auto const stale_safe_config = MainWindowP0IntegrationTestAccess::safe_configuration (stale_window);
      auto const stale_safe_endpoints = MainWindowP0IntegrationTestAccess::configuration_has_safe_endpoints (stale_window);
      auto const stale_initial_idle = MainWindowP0IntegrationTestAccess::tx_idle (stale_window);
      std::fprintf (stderr,
                    "DETAIL STALE_INITIAL safe_configuration=%d safe_endpoints=%d tx_idle=%d iptt=%d tx=%d tune=%d btxok=%d ready=%d\n",
                    stale_safe_config, stale_safe_endpoints, stale_initial_idle, g_iptt,
                    MainWindowP0IntegrationTestAccess::transmitting (stale_window),
                    MainWindowP0IntegrationTestAccess::tune_flag (stale_window),
                    MainWindowP0IntegrationTestAccess::btxok (stale_window),
                    MainWindowP0IntegrationTestAccess::tx_when_ready (stale_window));
      failures += !check (stale_safe_config,
                          "stale-only process starts with services disabled and no decoder process");
      failures += !check (stale_safe_endpoints,
                          "stale-only process uses Dummy/VOX with all CAT/PTT endpoints empty");
      failures += !check (stale_initial_idle,
                          "stale-only process starts with all software TX flags idle");
      MainWindowP0IntegrationTestAccess::activate_ft8 (stale_window);
      MainWindowP0IntegrationTestAccess::rig_open (stale_window);
      bool const stale_rig_online = wait_until ([&] {
        return MainWindowP0IntegrationTestAccess::transceiver_online (stale_window)
          && MainWindowP0IntegrationTestAccess::rig_state_online (stale_window)
          && MainWindowP0IntegrationTestAccess::generation (stale_window)
               == MainWindowP0IntegrationTestAccess::state_generation (stale_window);
      }, 5000);
      failures += !check (stale_rig_online,
                          "stale-reopen test opens only the real software Dummy session");
      MainWindowP0IntegrationTestAccess::enable_tx_via_button (stale_window, true);
      auto const stale_boundary = next_slot_boundary (
        MainWindowP0IntegrationTestAccess::virtual_now_ms (stale_window), true);
      MainWindowP0IntegrationTestAccess::set_virtual_time_ms (stale_window, stale_boundary + 1500);

      bool stale_reopen_exercised {false};
      bool stale_callback_queued {false};
      bool stale_params_changed {false};
      bool stale_generation_changed {false};
      bool stale_callback_no_new_send {false};
      bool stale_callback_token_revoked {false};
      bool stale_reopen_left_tx_idle {false};
      bool stale_rig_open_left_tx_idle_before_callback {false};
      bool stale_callback_flags_unchanged {false};
      bool stale_rx_boundary_tick {false};
      bool stale_rx_boundary_recovered {false};
      bool stale_rx_off_dispatched {false};
      int poll_before {-1};
      int poll_after {-1};
      int sends_before_stale_callback {-1};
      int sends_after_stale_callback {-1};
      quint64 stale_old_generation {0};
      quint64 stale_new_generation {0};
      quint64 stale_old_token_serial {0};
      quint64 stale_old_token_generation {0};
      qint64 stale_log_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (stale_window)}.size ();
      bool flags_before_callback_tx {false};
      bool flags_before_callback_tune {false};
      bool flags_before_callback_btxok {false};
      bool flags_before_callback_ready {false};
      qint32 flags_before_callback_iptt {0};
      bool flags_before_callback_ptt0 {false};
      QMetaObject::Connection stale_hook = QObject::connect (
        MainWindowP0IntegrationTestAccess::gui_timer (stale_window), &QTimer::timeout,
        &stale_window, [&] {
          if (stale_reopen_exercised
              || !MainWindowP0IntegrationTestAccess::pending_ptt_start (stale_window)) return;

          // guiUpdate has made a real begin-gate request and the Dummy rig has
          // started the production PTT delay timer. Stay inside this timeout
          // turn so no GUI/timer callback can run between queueing startTx2 and
          // the synchronous rigOpen generation change.
          stale_reopen_exercised = true;
          sends_before_stale_callback = stale_signals.modulator_start;
          stale_old_generation = MainWindowP0IntegrationTestAccess::generation (stale_window);
          stale_old_token_serial = MainWindowP0IntegrationTestAccess::token_serial (stale_window);
          stale_old_token_generation = MainWindowP0IntegrationTestAccess::token_generation (stale_window);
          MainWindowP0IntegrationTestAccess::print_stale_snapshot (
            stale_window, "before_reopen", sends_before_stale_callback);
          stale_callback_queued = QMetaObject::invokeMethod (
            &stale_window, "startTx2", Qt::QueuedConnection);
          stale_params_changed = MainWindowP0IntegrationTestAccess::change_poll_interval (
            stale_window, &poll_before, &poll_after);

          // valueChanged marks Configuration's rig invariants. The fixture
          // explicitly invokes the real MainWindow entry; it is not asserting
          // that a user valueChanged signal alone reopens the rig.
          MainWindowP0IntegrationTestAccess::rig_open (stale_window);
          stale_new_generation = MainWindowP0IntegrationTestAccess::generation (stale_window);
          stale_generation_changed = stale_new_generation != stale_old_generation;
          stale_rig_open_left_tx_idle_before_callback =
            MainWindowP0IntegrationTestAccess::tx_idle (stale_window)
            && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (stale_window)
            && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window);
          flags_before_callback_tx = MainWindowP0IntegrationTestAccess::transmitting (stale_window);
          flags_before_callback_tune = MainWindowP0IntegrationTestAccess::tune_flag (stale_window);
          flags_before_callback_btxok = MainWindowP0IntegrationTestAccess::btxok (stale_window);
          flags_before_callback_ready = MainWindowP0IntegrationTestAccess::tx_when_ready (stale_window);
          flags_before_callback_iptt = g_iptt;
          flags_before_callback_ptt0 = MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window);
          MainWindowP0IntegrationTestAccess::print_stale_snapshot (
            stale_window, "after_rigOpen_before_callback", stale_signals.modulator_start);

          // Deliver queued MainWindow meta calls, including the old startTx2
          // slot, without allowing a timer tick or fabricating TX-state cleanup.
          QCoreApplication::sendPostedEvents (&stale_window, QEvent::MetaCall);
          sends_after_stale_callback = stale_signals.modulator_start;
          stale_callback_no_new_send = sends_after_stale_callback == sends_before_stale_callback;
          stale_callback_token_revoked = !MainWindowP0IntegrationTestAccess::token_active (stale_window)
            && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (stale_window);
          stale_callback_flags_unchanged =
            MainWindowP0IntegrationTestAccess::transmitting (stale_window) == flags_before_callback_tx
            && MainWindowP0IntegrationTestAccess::tune_flag (stale_window) == flags_before_callback_tune
            && MainWindowP0IntegrationTestAccess::btxok (stale_window) == flags_before_callback_btxok
            && MainWindowP0IntegrationTestAccess::tx_when_ready (stale_window) == flags_before_callback_ready
            && g_iptt == flags_before_callback_iptt
            && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window) == flags_before_callback_ptt0;
          stale_reopen_left_tx_idle = MainWindowP0IntegrationTestAccess::tx_idle (stale_window)
            && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (stale_window)
            && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window);
          MainWindowP0IntegrationTestAccess::print_stale_snapshot (
            stale_window, "after_queued_old_startTx2", sends_after_stale_callback);

        });
      bool const stale_request_seen = wait_until ([&] { return stale_reopen_exercised; }, 10000);
      QObject::disconnect (stale_hook);
      std::fprintf (stderr,
                    "DETAIL STALE_REOPEN_RESULT seen=%d queued=%d params_changed=%d poll_before=%d poll_after=%d old_gen=%llu new_gen=%llu old_token=%llu token_gen=%llu flags_before=%d/%d/%d/%d/%d/%d callback_no_send=%d token_revoked=%d flags_unchanged_by_callback=%d rigOpen_idle_before_callback=%d callback_idle=%d sends_before=%d sends_after=%d gui_ticks=%d\n",
                    stale_request_seen, stale_callback_queued, stale_params_changed,
                    poll_before, poll_after,
                    static_cast<unsigned long long> (stale_old_generation),
                    static_cast<unsigned long long> (stale_new_generation),
                    static_cast<unsigned long long> (stale_old_token_serial),
                    static_cast<unsigned long long> (stale_old_token_generation),
                    flags_before_callback_tx, flags_before_callback_tune, flags_before_callback_btxok,
                    flags_before_callback_ready, flags_before_callback_iptt, flags_before_callback_ptt0,
                    stale_callback_no_new_send, stale_callback_token_revoked,
                    stale_callback_flags_unchanged, stale_rig_open_left_tx_idle_before_callback,
                    stale_reopen_left_tx_idle,
                    sends_before_stale_callback, sends_after_stale_callback,
                    stale_signals.gui_timer_timeouts);
      failures += !check (stale_request_seen && stale_callback_queued && stale_params_changed
                          && poll_before >= 0 && poll_after != poll_before
                          && stale_old_token_serial != 0
                          && stale_old_token_generation == stale_old_generation
                          && stale_generation_changed && stale_callback_no_new_send
                          && stale_callback_token_revoked,
                          "old queued startTx2 sends no new audio and has no valid token/PTT-on timer after real rigOpen parameter-reopen generation change");
      // Keep EnableTX on through the real FT8 RX boundary. If rigOpen did not
      // immediately leave an idle state, the production guiUpdate/stopTx and
      // 200ms OFF chain must clear it at the slot boundary.
      auto const ticks_before_rx = stale_signals.gui_timer_timeouts;
      auto const ptt_off_before_rx = stale_signals.ptt_off_timer_timeouts;
      MainWindowP0IntegrationTestAccess::set_virtual_time_ms (
        stale_window, stale_boundary + 15000);
      stale_rx_boundary_tick = wait_until ([&] {
        return stale_signals.gui_timer_timeouts > ticks_before_rx;
      }, 2000);
      if (!stale_reopen_left_tx_idle)
        {
          bool const stale_rx_stop_scheduled = wait_until ([&] {
            return MainWindowP0IntegrationTestAccess::tx_idle (stale_window)
              && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window);
          }, 2000);
          stale_rx_off_dispatched = wait_until ([&] {
            return stale_signals.ptt_off_timer_timeouts > ptt_off_before_rx
              && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window)
              && !MainWindowP0IntegrationTestAccess::rig_state_ptt (stale_window);
          }, 2500);
          failures += !check (stale_rx_stop_scheduled && stale_rx_off_dispatched,
                              "real RX slot boundary clears pending-request state through guiUpdate, stopTx, and the 200ms stopTx2 OFF timer");
          auto const stale_stop_log = read_log_from (
            MainWindowP0IntegrationTestAccess::rig_control_log_path (stale_window), stale_log_offset);
          failures += !check_log_event (
            stale_stop_log, QStringLiteral ("tx_slot_end"),
            QStringLiteral ("detail=previous_btxok=1|post_transmitting=0|post_g_iptt=0"),
            "stale-reopen RX-boundary slot-end");
          failures += !check_log_event (
            stale_stop_log, QStringLiteral ("ptt_off_schedule"),
            QStringLiteral ("detail=off_delay_ms=200|readback_state=unknown|"
                            "evidence_scope=software_observation_not_rf_proof"),
            "stale-reopen RX-boundary OFF schedule");
          failures += !check_log_event (
            stale_stop_log, QStringLiteral ("ptt_off_dispatch"),
            QStringLiteral ("detail=dispatch=requested|readback_state=unknown|"
                            "evidence_scope=mainwindow_signal_only_not_worker_or_driver"),
            "stale-reopen RX-boundary OFF dispatch");
        }
      stale_rx_boundary_recovered = stale_rx_boundary_tick
        && MainWindowP0IntegrationTestAccess::tx_idle (stale_window)
        && !MainWindowP0IntegrationTestAccess::ptt_on_timer_active (stale_window)
        && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window)
        && MainWindowP0IntegrationTestAccess::enable_tx (stale_window)
        && stale_signals.modulator_start == sends_before_stale_callback;
      MainWindowP0IntegrationTestAccess::print_stale_snapshot (
        stale_window, "after_real_RX_boundary", stale_signals.modulator_start);
      failures += !check (stale_rx_boundary_recovered,
                          "real guiUpdate at the legal RX boundary leaves software TX flags/timers clear while EnableTX stays open and sendMessage remains unchanged");
      std::fprintf (stderr,
                    "DETAIL STALE_RX_BOUNDARY tick=%d rigOpen_idle_before_callback=%d post_callback_idle=%d off_dispatched=%d enable=%d sends=%d\n",
                    stale_rx_boundary_tick, stale_rig_open_left_tx_idle_before_callback,
                    stale_reopen_left_tx_idle, stale_rx_off_dispatched,
                    MainWindowP0IntegrationTestAccess::enable_tx (stale_window),
                    stale_signals.modulator_start);

      // Keep EnableTX armed after the RX-boundary cleanup and let the next
      // legal FT8 slot traverse the real begin gate and token/timer chain.
      auto const next_tx_boundary = next_slot_boundary (
        MainWindowP0IntegrationTestAccess::virtual_now_ms (stale_window), true);
      auto const next_tx_log_offset = QFileInfo {
        MainWindowP0IntegrationTestAccess::rig_control_log_path (stale_window)}.size ();
      auto const sends_before_next_slot = stale_signals.modulator_start;
      auto const token_before_next_slot = MainWindowP0IntegrationTestAccess::token_serial (stale_window);
      MainWindowP0IntegrationTestAccess::set_virtual_time_ms (
        stale_window, next_tx_boundary + 1500);
      bool const next_legal_tx_started = wait_until ([&] {
        return stale_signals.modulator_start == sends_before_next_slot + 1
          && MainWindowP0IntegrationTestAccess::tx_active (stale_window);
      }, 7000);
      std::fprintf (stderr,
                    "DETAIL STALE_NEXT_TX_START started=%d enable=%d tx=%d btxok=%d iptt=%d ready=%d token_before=%llu token_after=%llu generation=%llu state_generation=%llu sends_before=%d sends_after=%d\n",
                    next_legal_tx_started,
                    MainWindowP0IntegrationTestAccess::enable_tx (stale_window),
                    MainWindowP0IntegrationTestAccess::transmitting (stale_window),
                    MainWindowP0IntegrationTestAccess::btxok (stale_window), g_iptt,
                    MainWindowP0IntegrationTestAccess::tx_when_ready (stale_window),
                    static_cast<unsigned long long> (token_before_next_slot),
                    static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::token_serial (stale_window)),
                    static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::generation (stale_window)),
                    static_cast<unsigned long long> (MainWindowP0IntegrationTestAccess::state_generation (stale_window)),
                    sends_before_next_slot, stale_signals.modulator_start);
      failures += !check (stale_rx_boundary_recovered && next_legal_tx_started
                          && MainWindowP0IntegrationTestAccess::enable_tx (stale_window)
                          && MainWindowP0IntegrationTestAccess::generation (stale_window)
                               == MainWindowP0IntegrationTestAccess::state_generation (stale_window)
                          && MainWindowP0IntegrationTestAccess::token_serial (stale_window)
                               > token_before_next_slot
                          && stale_signals.modulator_start == sends_before_next_slot + 1,
                          "after RX cleanup the next legal slot re-enters the real begin gate, issues a fresh token, and emits one software sendMessage with EnableTX open");

      auto const next_dispatch_baseline = stale_signals.ptt_off_timer_timeouts;
      MainWindowP0IntegrationTestAccess::set_virtual_time_ms (
        stale_window, next_tx_boundary + 13800);
      bool const next_tx_stop_scheduled = wait_until ([&] {
        return MainWindowP0IntegrationTestAccess::tx_idle (stale_window)
          && MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window);
      }, 2000);
      bool const next_tx_off_dispatched = wait_until ([&] {
        return stale_signals.ptt_off_timer_timeouts > next_dispatch_baseline
          && !MainWindowP0IntegrationTestAccess::ptt_off_timer_active (stale_window)
          && !MainWindowP0IntegrationTestAccess::rig_state_ptt (stale_window);
      }, 2500);
      auto const next_tx_log = read_log_from (
        MainWindowP0IntegrationTestAccess::rig_control_log_path (stale_window), next_tx_log_offset);
      failures += !check (next_tx_stop_scheduled && next_tx_off_dispatched
                          && MainWindowP0IntegrationTestAccess::enable_tx (stale_window)
                          && stale_signals.modulator_start == sends_before_next_slot + 1,
                          "the recovered next legal TX slot also stops through the real 200ms OFF chain without disarming EnableTX or sending again");
      failures += !check_log_event (
        next_tx_log, QStringLiteral ("tx_slot_end"),
        QStringLiteral ("detail=previous_btxok=1|post_tx_when_ready=0|post_g_iptt=0"),
        "stale-only next legal TX slot-end");
      failures += !check_log_event (
        next_tx_log, QStringLiteral ("ptt_off_schedule"),
        QStringLiteral ("detail=off_delay_ms=200|post_tx_when_ready=0|"
                        "evidence_scope=software_observation_not_rf_proof"),
        "stale-only next legal TX OFF schedule");
      failures += !check_log_event (
        next_tx_log, QStringLiteral ("ptt_off_dispatch"),
        QStringLiteral ("detail=dispatch=requested|"
                        "evidence_scope=mainwindow_signal_only_not_worker_or_driver"),
        "stale-only next legal TX OFF dispatch");
      MainWindowP0IntegrationTestAccess::print_stale_snapshot (
        stale_window, "after_next_legal_TX_and_OFF", stale_signals.modulator_start);
      MainWindowP0IntegrationTestAccess::enable_tx_via_button (stale_window, false);
      pump_for (250);
      MainWindowP0IntegrationTestAccess::print_stale_snapshot (
        stale_window, "after_ui_disarm_250ms", stale_signals.modulator_start);
      failures += !check (stale_signals.modulator_start == sends_before_next_slot + 1
                          && !MainWindowP0IntegrationTestAccess::enable_tx (stale_window),
                          "UI disarm prevents future legal slot requests after stale callback evidence is captured");
      failures += !check (0 == stale_signals.audio_input_start
                          && 0 == stale_signals.audio_output_initialize,
                          "stale-reopen path emits no real audio stream initialization/start signals");
      stale_window.close ();
      pump_for (150);
  }
  if (stale_shared_memory.isAttached ()) stale_shared_memory.detach ();
    }
  shared_memory.detach ();
  if (failures)
    {
      std::fprintf (stderr, "FAIL: %d MainWindow integration assertions failed\n", failures);
      return 1;
    }
  if (stale_only)
    std::puts ("PASS: independent real MainWindow stale-token reopen, RX-boundary stop, and next legal TX slot");
  else
    std::puts ("PASS: real MainWindow FT8 phases/cycles, TX/RX recovery, manual/Tune/fresh-OFF stops, logging and active-TX shutdown");
  return 0;
}
