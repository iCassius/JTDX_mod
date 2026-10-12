#include "ptt_start_policy.hpp"

#include <QObject>
#include <QHash>
#include <QString>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int g_iptt = 0;

namespace
{
  void require (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "FAIL " << description << '\n';
        std::exit (EXIT_FAILURE);
      }
    std::cout << "PASS " << description << '\n';
  }
}

struct FakeTimer
{
  bool active {false};
  int last_delay_ms {-1};
  int starts {0};
  int stops {0};

  void start (int delay_ms = 0)
  {
    active = true;
    last_delay_ms = delay_ms;
    ++starts;
  }
  void stop ()
  {
    active = false;
    ++stops;
  }
  bool isActive () const { return active; }
  int interval () const { return last_delay_ms; }
};

struct FakeWidget
{
  bool disabled {false};
  bool checked {false};
  int value_ {0};
  QString text_;

  void setDisabled (bool value) { disabled = value; }
  void setStyleSheet (QString const&) {}
  QString styleSheet () const { return {}; }
  void setText (QString const& value) { text_ = value; }
  void setChecked (bool value) { checked = value; }
  int value () const { return value_; }
  void setValue (int value) { value_ = value; }
};

struct FakeCombo
{
  QString currentText () const { return QStringLiteral ("20m"); }
};

struct FakeUi
{
  FakeWidget txFrequency;
  FakeWidget tuneButtonStorage;
  FakeWidget attenuation;
  FakeCombo band;
  FakeWidget * TxFreqSpinBox {&txFrequency};
  FakeWidget * tuneButton {&tuneButtonStorage};
  FakeWidget * outAttenuation {&attenuation};
  FakeCombo * bandComboBox {&band};
};

struct FakeTime
{
  qint64 currentMSecsSinceEpoch2 () const { return 1000; }
};

struct FakeConfig
{
  std::uint64_t generation {17};
  int ptt_off_calls {0};
  int tune_on_calls {0};
  int tune_off_calls {0};
  int revoke_calls {0};

  std::uint64_t transceiver_session_generation () const { return generation; }
  void transceiver_ptt (bool on)
  {
    if (!on) ++ptt_off_calls;
  }
  void transceiver_modulator_stop () {}
  void transceiver_tune (bool on)
  {
    if (on) ++tune_on_calls;
    else ++tune_off_calls;
  }
  void revoke_transceiver_ptt_start () { ++revoke_calls; }
  bool pwrBandTuneMemory () const { return false; }
  bool pwrBandTxMemory () const { return false; }
  int tunetimer () const { return 0; }
  bool write_decoded_debug () const { return false; }
};

struct FakeSequenceState
{
  bool active_ {false};
  bool active () const { return active_; }
  bool pending () const { return false; }
};

struct FakeHistory
{
  void remove (QString const&) {}
};

struct FakeWideGraph
{
  void setWSPRtransmitted () {}
};

struct RigObservation
{
  bool online_ {true};
  bool ptt_ {false};
  bool readback_required_ {true};
  bool readback_known_ {true};
  std::uint64_t serial_ {0};

  bool online () const { return online_; }
  bool ptt () const { return ptt_; }
  bool ptt_readback_required () const { return readback_required_; }
  bool ptt_readback_known () const { return readback_known_; }
  std::uint64_t ptt_readback_serial () const { return serial_; }
};

class MainWindow
{
public:
  struct PttStopFlags
  {
    bool enable_tx;
    bool btxok;
    bool transmitting;
    bool tune;
    bool tx_when_ready;
    int g_iptt;
  };
  struct PttStopDiagnostic
  {
    QString event;
    QString detail_name;
    int detail_value;
    PttStopFlags before;
    PttStopFlags after;
  };

  MainWindow ()
    : ui (&ui_storage), tx_status_label (&tx_status_storage),
      last_tx_label (&last_tx_storage), m_jtdxtime (&time_storage),
      m_wideGraph (&wide_graph_storage)
  {}

  void on_stopTxButton_clicked ();
  void stop_tuning ();
  void on_tuneButton_clicked (bool checked);
  void haltTx (QString reason);
  void haltTxTuneTimer ();
  void stopTx ();
  void stopTx2 ();
  void production_gui_stop_edge (bool btxok0);
  bool production_fresh_rig_off (RigObservation const& s,
                                 bool pttReadbackAdvanced);

  void cancelPttStartAndRevokeAdmission (QString const&, QString const& = {})
  {
    ++m_cancel_calls;
    m_config.revoke_transceiver_ptt_start ();
    m_pttStartPolicy.cancel ();
  }
  void cancelAutoSeqInitialCallStall (QString const&, bool)
  { m_autoSeqInitialCallStall.active_ = false; }
  void setAutoSeqInitialCallOperatorHold (bool hold, QString const&,
                                          QString const& = {})
  {
    // This extracted-source stub models the production state assignment;
    // diagnostic logging is outside this stop-path fixture's scope.
    m_autoSeqInitialCallOperatorHold = hold;
  }
  void appendAutoSeqDiagnostic (QString const& event, QString const&,
                                QString const& = {}, bool = false, int = 0)
  {
    if (event == QStringLiteral ("halt_tx_requested"))
      ++m_haltTxRequestedCount;
  }
  void writeHaltTxEvent (QString const&) {}
  void cancelAutoSeqRecovery (QString const&) {}
  void enableTx_mode (bool) {}
  void autoFilter (bool) {}
  void endTransmitMessage () {}
  void monitor (bool) {}
  void statusUpdate () {}
  void WSPR_scheduling () {}
  void writeToALLTXT (QString const&) {}
  void appendPttStopDiagnostic (char const * event,
                                PttStopFlags const& before,
                                PttStopFlags const& after,
                                char const * detail_name,
                                int detail_value) noexcept
  {
    ++m_stopDiagnosticAttempts;
    try
      {
        if (m_failStopDiagnostics) return;
        m_stopDiagnostics.push_back ({QString::fromLatin1 (event),
                                      QString::fromLatin1 (detail_name),
                                      detail_value, before, after});
      }
    catch (...)
      {
        // Match production's no-throw diagnostic boundary.
      }
  }
  QString tr (char const * text) const { return QString::fromLatin1 (text); }
  void tune (bool checked)
  {
    if (checked) ++m_config.tune_on_calls;
    else ++m_config.tune_off_calls;
  }

  FakeUi ui_storage;
  FakeUi * ui;
  FakeWidget tx_status_storage;
  FakeWidget * tx_status_label;
  FakeWidget last_tx_storage;
  FakeWidget * last_tx_label;
  FakeTime time_storage;
  FakeTime * m_jtdxtime;
  FakeWideGraph wide_graph_storage;
  FakeWideGraph * m_wideGraph;
  FakeConfig m_config;
  FakeTimer ptt0Timer;
  FakeTimer ptt1Timer;
  FakeTimer tuneButtonTimer;
  FakeTimer StopTuneTimer;
  FakeSequenceState m_autoSeqInitialCallStall;
  FakeSequenceState m_autoSeqRecovery;
  FakeHistory m_qsoHistory;
  PttStartPolicy m_pttStartPolicy;
  PttStartPolicy::Token m_pttRequestToken {};
  RigObservation m_rigState;
  QHash<QString, QString> m_pwrBandTuneMemory;
  QHash<QString, QString> m_pwrBandTxMemory;
  QString m_mode {QStringLiteral ("FT8")};
  QString m_modeTx {QStringLiteral ("FT8")};
  QString m_hisCall;
  QString m_currentMessage {QStringLiteral ("CQ TEST")};
  bool m_autoSeqInternalHalt {false};
  bool m_autoSeqInitialCallOperatorHold {false};
  bool m_autoSeqInternalEnableTxChange {false};
  bool m_autoSeqRecoveryInternalHalt {false};
  bool m_autoDirectedAnswerActive {false};
  bool m_transmitting {false};
  bool m_tune {false};
  bool m_tuneStopPending {false};
  bool m_enableTx {false};
  bool m_tuneup {false};
  bool m_btxok {false};
  bool m_haltTrans {false};
  bool m_haltTxWritten {false};
  bool m_tci {false};
  bool m_txwatchdog {false};
  bool m_monitoroff {false};
  bool m_tx_when_ready {false};
  bool m_restart {false};
  bool m_txNext {false};
  bool m_filter {false};
  bool m_skipTx1 {false};
  bool m_PwrBandSetOK {true};
  bool m_sentFirst73 {false};
  bool m_ftx1PttReadinessRejected {false};
  bool m_ftx1ReadinessAutoDisabled {false};
  int m_ftx1ReadinessRejectedBatchId {0};
  bool m_bTxTime {false};
  int m_FilterState {0};
  int m_addtx {0};
  int m_nlasttx {0};
  int m_ntx {1};
  int m_QSOProgress {0};
  int m_ntr {0};
  int m_secTxStopped {0};
  int m_pttRequestedAtMs {0};
  int m_cancel_calls {0};
  int m_haltTxRequestedCount {0};
  int m_stopDiagnosticAttempts {0};
  bool m_failStopDiagnostics {false};
  std::vector<PttStopDiagnostic> m_stopDiagnostics;
  int itone[8] {};
};

#define REPORT 2
#include "p0_tx_stop_methods_generated.inc"

int main ()
{
  MainWindow manual;
  bool const previous_btxok = true;
  manual.m_autoSeqInitialCallStall.active_ = true;
  manual.m_transmitting = true;
  manual.m_btxok = true;
  g_iptt = 1;
  manual.on_stopTxButton_clicked ();
  require (!manual.m_btxok && !manual.m_transmitting && g_iptt == 1,
           "manual stop clears TX intent and waits for the prior GUI falling edge");
  require (manual.m_autoSeqInitialCallOperatorHold
             && !manual.m_autoSeqInitialCallStall.active (),
           "manual stop preserves operator hold while canceling the active AutoSeq owner");
  manual.production_gui_stop_edge (previous_btxok);
  require (g_iptt == 0 && !manual.m_transmitting
             && manual.ptt0Timer.isActive ()
             && manual.ptt0Timer.last_delay_ms == 200,
           "production GUI falling edge enters stopTx and schedules one delayed OFF");
  require (manual.m_stopDiagnostics.size () == 2
             && manual.m_stopDiagnostics[0].event == QStringLiteral ("ptt_off_schedule")
             && manual.m_stopDiagnostics[0].detail_name == QStringLiteral ("off_delay_ms")
             && manual.m_stopDiagnostics[0].detail_value == 200
             && manual.m_stopDiagnostics[1].event == QStringLiteral ("tx_slot_end")
             && manual.m_stopDiagnostics[1].before.g_iptt == 1
             && manual.m_stopDiagnostics[1].after.g_iptt == 0,
           "stop scheduling and slot-end logs capture one software pre/post transition");
  manual.stopTx2 ();
  require (manual.m_config.ptt_off_calls == 1 && !manual.ptt1Timer.isActive (),
           "production stopTx2 sends the rig PTT OFF request");
  require (manual.m_stopDiagnostics.size () == 3
             && manual.m_stopDiagnostics[2].event == QStringLiteral ("ptt_off_dispatch")
             && manual.m_stopDiagnostics[2].detail_name == QStringLiteral ("dispatch_result")
             && manual.m_stopDiagnostics[2].detail_value == 1,
           "stopTx2 records one UI dispatch request distinct from driver/readback success");

  MainWindow failed_log;
  failed_log.m_failStopDiagnostics = true;
  failed_log.m_btxok = false;
  failed_log.m_transmitting = false;
  g_iptt = 1;
  failed_log.production_gui_stop_edge (true);
  failed_log.stopTx2 ();
  require (g_iptt == 0 && failed_log.ptt0Timer.isActive ()
             && failed_log.ptt0Timer.last_delay_ms == 200
             && failed_log.m_config.ptt_off_calls == 1
             && failed_log.m_stopDiagnosticAttempts == 3
             && failed_log.m_stopDiagnostics.empty (),
           "diagnostic write failure leaves timer scheduling and OFF dispatch intact");

  MainWindow tune;
  tune.m_transmitting = true;
  tune.m_btxok = true;
  g_iptt = 1;
  tune.on_tuneButton_clicked (true);
  require (tune.m_tune && tune.m_config.tune_on_calls == 1,
           "Tune start reaches the configured tune request");
  tune.on_tuneButton_clicked (false);
  require (tune.m_tuneStopPending && !tune.ptt1Timer.isActive ()
             && tune.tuneButtonTimer.isActive ()
             && tune.tuneButtonTimer.last_delay_ms == 250
             && !PttStartPolicy::may_start_request (true, tune.m_tuneStopPending),
           "Tune stop closes new start admission during the 250 ms handoff");
  tune.haltTxTuneTimer ();
  require (!tune.m_tune && !tune.m_tuneStopPending && !tune.m_bTxTime
             && !tune.m_transmitting,
           "Tune timeout converges through the production stop button and stop_tuning");
  tune.production_gui_stop_edge (true);
  require (g_iptt == 0 && tune.ptt0Timer.last_delay_ms == 200,
           "Tune stop reaches the same delayed rig OFF path");
  tune.stopTx2 ();
  require (tune.m_config.ptt_off_calls == 1,
           "Tune stop timer callback reaches production stopTx2 OFF");

  MainWindow rig;
  rig.m_transmitting = true;
  rig.m_btxok = true;
  g_iptt = 1;
  rig.m_config.generation = 23;
  rig.m_pttRequestToken = rig.m_pttStartPolicy.begin (23, 4, true);
  RigObservation on;
  on.ptt_ = true;
  on.serial_ = 5;
  require (!rig.production_fresh_rig_off (on, true)
             && rig.m_haltTxRequestedCount == 0,
           "fresh same-generation PTT ON observation does not halt TX");
  RigObservation off;
  off.ptt_ = false;
  off.serial_ = 6;
  require (rig.production_fresh_rig_off (off, true)
             && rig.m_haltTxRequestedCount == 1
             && !rig.m_transmitting && !rig.m_btxok,
           "fresh same-generation ON-to-OFF readback reaches production halt");
  rig.production_gui_stop_edge (true);
  require (g_iptt == 0 && rig.ptt0Timer.last_delay_ms == 200,
           "rig early-OFF halt converges through stopTx to delayed OFF");
  rig.stopTx2 ();
  require (rig.m_config.ptt_off_calls == 1,
           "rig early-OFF path retains the terminal OFF request");

  MainWindow stale;
  stale.m_transmitting = true;
  stale.m_btxok = true;
  g_iptt = 1;
  stale.m_config.generation = 24;
  stale.m_pttRequestToken = stale.m_pttStartPolicy.begin (23, 4, true);
  require (!stale.production_fresh_rig_off (off, true)
             && stale.m_haltTxRequestedCount == 0
             && stale.m_transmitting,
           "expired prior-generation PTT callback cannot halt or rewrite current TX state");

  std::cout << "production_source_hash="
            << P0_TX_STOP_SOURCE_SHA256
            << " fake_backend_off_calls=manual:1,tune:1,rig:1\n";
  return EXIT_SUCCESS;
}
