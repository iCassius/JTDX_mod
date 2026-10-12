#include <QFile>
#include <QString>

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  QString source (QString const& relativePath)
  {
    QFile file {QStringLiteral (JTDX_SOURCE_DIR) + QLatin1Char ('/')
                + relativePath};
    require (file.open (QIODevice::ReadOnly), "source contract fixture opens");
    return QString::fromUtf8 (file.readAll ());
  }

  bool ordered (QString const& text, QString const& first, QString const& second)
  {
    auto const firstIndex = text.indexOf (first);
    return firstIndex >= 0 && text.indexOf (second, firstIndex + first.size ()) >= 0;
  }
}

int main ()
{
  auto const hamlib = source (QStringLiteral ("HamlibTransceiver.cpp"));
  auto const polling = source (QStringLiteral ("PollingTransceiver.cpp"));
  auto const transceiver = source (QStringLiteral ("TransceiverBase.cpp"));
  auto const configuration = source (QStringLiteral ("Configuration.cpp"));
  auto const mainwindow = source (QStringLiteral ("mainwindow.cpp"));
  auto const localLog = source (QStringLiteral ("JtdxLocalLog.cpp"));
  auto const rigPolicy = source (QStringLiteral ("RigSessionPolicy.hpp"));
  auto const updatePolicy = source (QStringLiteral ("transceiver_update_policy.hpp"));
  auto const pttStartPolicy = source (QStringLiteral ("ptt_start_policy.hpp"));
  auto const txPayload = source (QStringLiteral ("ptt_tx_payload.hpp"));
  auto const transceiverState = source (QStringLiteral ("Transceiver.hpp"));
  auto const pttObservationPolicy = source (
    QStringLiteral ("autoseq_ptt_observation_policy.hpp"));

  require (configuration.contains (QStringLiteral ("readback_unknown"))
               && !configuration.contains (QStringLiteral ("write_state_unconfirmed")),
           "unknown strict PTT source is not mislabeled as an unconfirmed write");

  require (hamlib.contains (QStringLiteral ("throws_to_offline=%14"))
               && hamlib.contains (QStringLiteral (
                    "Decision::soft_ignore != decision ? \"true\" : \"false\"")),
           "hard CAT poll outcomes are marked for the existing offline path");
  require (ordered (polling,
                    QStringLiteral ("poll_exception offline_transition=begin"),
                    QStringLiteral ("offline (message);")),
           "poll exception diagnostic precedes the existing offline transition");
  require (ordered (transceiver,
                    QStringLiteral ("will_emit_this_call"),
                    QStringLiteral ("Q_EMIT failure (reason);"))
               && ordered (transceiver,
                           QStringLiteral ("Q_EMIT failure (reason);"),
                           QStringLiteral ("emitted_this_call"))
               && transceiver.contains (QStringLiteral ("already_emitted"))
               && transceiver.contains (QStringLiteral ("retired_command_ignored action=%1")),
           "offline emission truth and retired command actions are persisted for Release diagnostics");

  auto const configurationFailureStart = configuration.indexOf (
    QStringLiteral ("void Configuration::impl::handle_transceiver_failure"));
  auto const configurationFailureEnd = configuration.indexOf (
    QStringLiteral ("void Configuration::impl::close_rig ("), configurationFailureStart);
  auto const configurationFailure = configuration.mid (
    configurationFailureStart, configurationFailureEnd - configurationFailureStart);
  auto const failureDiagnosticStart = configuration.indexOf (
    QStringLiteral ("void append_configuration_failure_diagnostic"));
  auto const failureDecisionStart = configuration.indexOf (
    QStringLiteral ("void append_configuration_failure_decision"), failureDiagnosticStart);
  auto const failureDiagnostic = configuration.mid (
    failureDiagnosticStart, failureDecisionStart - failureDiagnosticStart);
  require (configurationFailure.contains (QStringLiteral (
               "if (generation != rig_generation_)"))
               && ordered (configurationFailure,
                           QStringLiteral ("if (generation != rig_generation_)"),
                           QStringLiteral ("append_configuration_failure_diagnostic (\"stale_generation_ignored\", generation,"))
               && ordered (configurationFailure,
                           QStringLiteral ("if (generation != rig_generation_)"),
                           QStringLiteral ("RigSessionPolicy::failure_action"))
               && configurationFailure.contains (QStringLiteral (
                    "RigSessionPolicy::failure_action")),
           "stale configuration generations are rejected before failure routing");
  require (ordered (configurationFailure,
                    QStringLiteral ("RigSessionPolicy::failure_action"),
                    QStringLiteral ("append_configuration_failure_diagnostic (\"entered\", generation,"))
               && ordered (configurationFailure,
                           QStringLiteral ("append_configuration_failure_diagnostic (\"entered\", generation,"),
                           QStringLiteral ("close_rig (\"transceiver_failure\", \"handle_transceiver_failure\");"))
               && ordered (configurationFailure,
                           QStringLiteral ("close_rig (\"transceiver_failure\", \"handle_transceiver_failure\");"),
                           QStringLiteral ("append_configuration_failure_diagnostic (\"after_close_rig\", generation,"))
               , "configuration failure lifecycle logs before and after close");
  require (failureDiagnostic.contains (QStringLiteral ("current_generation=%3"))
               && failureDiagnostic.contains (QStringLiteral ("session_purpose=%4"))
               && failureDiagnostic.contains (QStringLiteral ("filter_cause=%5")),
           "configuration failure helper records generation, purpose, and filter cause");
  require (configurationFailure.contains (QStringLiteral (
                    "FailureAction::forward_to_runtime == action"))
               && configurationFailure.contains (QStringLiteral ("if (forward_to_main_window)"))
               && configurationFailure.contains (QStringLiteral ("Q_EMIT self_->transceiver_failure (reason);"))
               && configurationFailure.contains (QStringLiteral ("message_box_critical (tr (\"Rig failure\"), reason);"))
               && !configurationFailure.contains (QStringLiteral ("isVisible ()")),
           "Configuration routes runtime and test failures by session purpose");
  require (configuration.contains (QStringLiteral (
               "connect (rig.get (), &Transceiver::failure, this,"))
               && configuration.contains (QStringLiteral ("[this, generation]"))
               && configuration.contains (QStringLiteral (
                    "handle_transceiver_failure (reason, generation);"))
               && configuration.contains (QStringLiteral (
                    "open_rig (true, RigSessionPurpose::configuration_test)"))
               && configuration.contains (QStringLiteral (
                    "open_rig (false, RigSessionPurpose::configuration_test)"))
               && configuration.contains (QStringLiteral (
                    "rig_session_purpose_ = purpose;"))
               && configuration.contains (QStringLiteral (
                    "RigSessionPolicy::purpose_after_reuse"))
               && configuration.contains (QStringLiteral (
                    "rig_session_purpose_ = RigSessionPurpose::runtime;")),
           "Configuration binds failure callbacks, open paths, and runtime adoption to session purpose and generation");
  auto const configurationUpdateStart = configuration.indexOf (
    QStringLiteral ("void Configuration::impl::handle_transceiver_update"));
  auto const configurationFailureHandler = configuration.indexOf (
    QStringLiteral ("void Configuration::impl::handle_transceiver_failure"),
    configurationUpdateStart);
  auto const configurationUpdate = configuration.mid (
    configurationUpdateStart, configurationFailureHandler - configurationUpdateStart);
  require (configurationUpdateStart >= 0 && configurationFailureHandler > configurationUpdateStart
               && ordered (configurationUpdate,
                           QStringLiteral ("TransceiverUpdatePolicy::classify"),
                           QStringLiteral ("cached_rig_state_.online (true)"))
               && ordered (configurationUpdate,
                           QStringLiteral ("close_rig (\"offline_update\", \"handle_transceiver_update\");"),
                           QStringLiteral ("Q_EMIT self_->transceiver_update (offline_state);"))
               && ordered (configurationUpdate,
                           QStringLiteral ("Q_EMIT self_->transceiver_update (offline_state);"),
                           QStringLiteral ("Q_EMIT self_->transceiver_failure (reason);"))
               && configurationUpdate.contains (QStringLiteral ("RigSessionPurpose::runtime == purpose")),
           "confirmed offline routes once after generation close, with MainWindow state before its failure recovery");
  require (updatePolicy.contains (QStringLiteral ("update_generation != current_generation"))
               && updatePolicy.contains (QStringLiteral ("update_sequence == current_sequence"))
               && updatePolicy.contains (QStringLiteral ("report_confirmed_offline")),
           "the production update classifier checks object generation before command sequence and preserves terminal offline");
  require (rigPolicy.contains (QStringLiteral ("failure_action"))
               && rigPolicy.contains (QStringLiteral ("purpose_after_reuse")),
           "Configuration uses the independently tested rig-session policy");
  require (mainwindow.contains (QStringLiteral (
               "connect (&m_config, &Configuration::transceiver_failure, this, &MainWindow::handle_transceiver_failure);")),
           "MainWindow failure connection remains unchanged");
  auto const mainwindowUpdateStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::handle_transceiver_update (Transceiver::TransceiverState const& s)"));
  auto const mainwindowDiagnosticsStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"), mainwindowUpdateStart);
  auto const mainwindowUpdate = mainwindow.mid (
    mainwindowUpdateStart, mainwindowDiagnosticsStart - mainwindowUpdateStart);
  auto const offlineEarlyReturn = mainwindowUpdate.indexOf (QStringLiteral ("if (!s.online ())"));
  auto const offlineReturn = mainwindowUpdate.indexOf (QStringLiteral ("return;"), offlineEarlyReturn);
  require (offlineEarlyReturn >= 0 && offlineReturn > offlineEarlyReturn
               && !mainwindowUpdate.mid (offlineEarlyReturn,
                                         offlineReturn - offlineEarlyReturn)
                    .contains (QStringLiteral ("haltTx ("))
               && !mainwindowUpdate.mid (offlineEarlyReturn,
                                         offlineReturn - offlineEarlyReturn)
                    .contains (QStringLiteral ("m_freqNominal = s.frequency"))
               && mainwindowUpdate.contains (QStringLiteral ("m_rigState.online (false)"))
               && mainwindowUpdate.contains (QStringLiteral ("m_rigOk = false")),
           "offline snapshots invalidate online/PTT state and return before TX halting or default-frequency side effects");
  auto const diagnosticFormatterStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"));
  auto const diagnosticFormatterEnd = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::cancelAutoSeqInitialCallStall"),
    diagnosticFormatterStart);
  auto const diagnosticFormatter = mainwindow.mid (
    diagnosticFormatterStart, diagnosticFormatterEnd - diagnosticFormatterStart);
  require (diagnosticFormatter.contains (QStringLiteral ("AutoSeqDiagnosticFormatter::append_field"))
               && !diagnosticFormatter.contains (QStringLiteral ("%31"))
               && !diagnosticFormatter.contains (QStringLiteral ("auto_seq_log_value"))
               && diagnosticFormatter.contains (QStringLiteral ("requested_context_valid"))
               && diagnosticFormatter.contains (QStringLiteral ("request_context_diff"))
               && diagnosticFormatter.contains (QStringLiteral ("tx_payload_changed"))
               && diagnosticFormatter.contains (QStringLiteral ("ptt_owner_call"))
               && diagnosticFormatter.contains (QStringLiteral ("operator_hold"))
               && diagnosticFormatter.contains (QStringLiteral ("ftx1_readiness_rejected_batch_id"))
               && diagnosticFormatter.contains (QStringLiteral ("recovery_phase"))
               && diagnosticFormatter.contains (QStringLiteral ("recovery_pending"))
               && !diagnosticFormatter.contains (QStringLiteral ("write_state_unconfirmed"))
               && !diagnosticFormatter.contains (QStringLiteral (
                    "add (QStringLiteral (\"tx_payload_identity\")")),
           "AutoSeq diagnostic values are encoded by independent fields instead of chained positional placeholders");
  require (diagnosticFormatter.contains (QStringLiteral ("pttRequestDiagnostic"))
               && ordered (diagnosticFormatter,
                           QStringLiteral ("if (pttRequestDiagnostic)"),
                           QStringLiteral ("requested_context_valid")),
           "request context detail is limited to PTT request/refusal events instead of normal readback metadata");
  require (diagnosticFormatter.contains (QStringLiteral (
               "if (includeTxText && !pttRequestDiagnostic)"))
               && !diagnosticFormatter.contains (QStringLiteral (
                    "add (QStringLiteral (\"tx_payload_identity\")")),
           "PTT request diagnostics expose payload changes only as a boolean and do not emit payload text or identity bytes");

  require (mainwindow.contains (QStringLiteral ("recovery_ticket decision=%1 reason=%2"))
               && mainwindow.contains (QStringLiteral ("history_status=%8"))
               && mainwindow.contains (QStringLiteral ("retry_count=%9"))
               && mainwindow.contains (QStringLiteral ("tx_when_ready=%13"))
               && mainwindow.contains (QStringLiteral ("no_target"))
               && mainwindow.contains (QStringLiteral ("no_active_tx_intent"))
               && mainwindow.contains (QStringLiteral ("auto_sequence_unsupported"))
               && mainwindow.contains (QStringLiteral ("target_missing_after_disconnect")),
           "ticket decision log includes target/status/count/intent and rejection reasons");
  require (mainwindow.contains (QStringLiteral (
               "recovery_online_update online=true ptt_known=%1 ptt_on=%2"))
               && mainwindow.contains (QStringLiteral ("release_dx_wait_fresh_decode"))
               && mainwindow.contains (QStringLiteral ("retain_ticket_and_dx"))
               && mainwindow.contains (QStringLiteral ("cancel_ticket_context_changed"))
               && mainwindow.contains (QStringLiteral (
                    "decision=%7")),
           "online recovery logs PTT readback and release/retain/cancel decisions");
  require (ordered (mainwindow,
                    QStringLiteral ("m_autoSeqRecovery.reconnected_ptt_off (recoveredAt);"),
                    QStringLiteral ("clearDX (\" cleared after CAT recovery; waiting for fresh decode\");")),
           "existing PTT-off gate still precedes DX release");
  require (mainwindow.count (QStringLiteral ("m_autoSeqRecovery.cancel ();")) == 1,
           "all recovery ticket cancellations pass through the diagnostic wrapper");
  auto const cancelStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::cancelAutoSeqRecovery"));
  auto const cancelMethod = mainwindow.mid (cancelStart, 1800);
  require (ordered (cancelMethod, QStringLiteral ("try"),
                    QStringLiteral ("m_qsoHistory.diagnosticSnapshot"))
               && ordered (cancelMethod, QStringLiteral ("catch (...)"),
                           QStringLiteral ("m_autoSeqRecovery.cancel ();")),
           "diagnostic preparation is caught and the original cancel always executes");
  require (mainwindow.contains (QStringLiteral ("void appendRecoveryLogLazy"))
               && mainwindow.contains (QStringLiteral (
                    "Formatting and diagnostic writes must not affect MainWindow control flow.")),
           "new MainWindow diagnostic formatting is inside a no-throw wrapper");
  require (mainwindow.contains (QStringLiteral ("reason_present=true"))
               && !mainwindow.contains (QStringLiteral ("failure=%1; online=%2")),
           "failure diagnostics record reason presence without logging CAT error text");
  require (localLog.contains (QStringLiteral ("catch (...)"))
               && localLog.contains (QStringLiteral ("Diagnostics must never escape into CAT, AutoSeq, or TX control paths.")),
           "shared diagnostic append catches formatting, lock, and filesystem exceptions");
  auto const hamlibPollStart = hamlib.indexOf (QStringLiteral ("void HamlibTransceiver::do_poll ()"));
  auto const nextHamlibFunction = hamlib.indexOf (QStringLiteral ("void HamlibTransceiver::"),
                                                  hamlibPollStart + 1);
  auto const hamlibPoll = hamlib.mid (hamlibPollStart, nextHamlibFunction - hamlibPollStart);
  require (hamlibPoll.contains (QStringLiteral ("ftx1_poll_has_successful_read_ = false;"))
               && ordered (hamlibPoll,
                           QStringLiteral ("else if (ftx1_poll_has_successful_read_)"),
                           QStringLiteral ("observe_poll_success (Ftx1CatPollPolicy::Operation::protocol_sync)"))
               && ordered (hamlibPoll,
                           QStringLiteral ("observe_poll_success (Ftx1CatPollPolicy::Operation::protocol_sync)"),
                           QStringLiteral ("ftx1_poll_policy_.complete_poll ();")),
           "FTX-1 protocol-sync success is credited only after this poll performed a real successful read");

  auto const ftxPttGet = hamlibPoll.indexOf (QStringLiteral ("auto const readback_generation"));
  auto const ftxPttRead = hamlibPoll.mid (ftxPttGet);
  require (ftxPttGet >= 0
               && ordered (ftxPttRead,
                           QStringLiteral ("auto const rc = rig_get_ptt"),
                           QStringLiteral ("generation_after_readback"))
               && ordered (ftxPttRead,
                           QStringLiteral ("has_new_protocol_sync_events"),
                           QStringLiteral ("if (RIG_OK == rc)"))
               && ordered (ftxPttRead,
                           QStringLiteral ("if (RIG_OK == rc)"),
                           QStringLiteral ("confirm_PTT_readback (ptt_on)"))
               && ftxPttRead.contains (QStringLiteral ("if (ptt_readback_required_)")),
           "only a successful FTX-1 CAT GET_PTT without a new wrong-reply event advances its readback serial");
  auto const pttWriteStart = hamlib.indexOf (QStringLiteral ("void HamlibTransceiver::do_ptt (bool on)"));
  auto const pttWriteEnd = hamlib.indexOf (QStringLiteral ("void HamlibTransceiver::"), pttWriteStart + 1);
  auto const pttWrite = hamlib.mid (pttWriteStart, pttWriteEnd - pttWriteStart);
  require (ordered (pttWrite,
                    QStringLiteral ("update_PTT_readback_known (false)"),
                    QStringLiteral ("rig_set_ptt")),
           "a PTT write invalidates actual readback before issuing the optimistic command");

  auto const requestStart = mainwindow.indexOf (
    QStringLiteral ("if(g_iptt==0 && PttStartPolicy::may_start_request"));
  auto const requestEnd = mainwindow.indexOf (
    QStringLiteral ("if(!m_bTxTime and !m_tune) m_btxok=false"), requestStart);
  auto const requestPath = mainwindow.mid (requestStart, requestEnd - requestStart);
  require (requestStart >= 0 && requestEnd > requestStart
               && ordered (requestPath,
                           QStringLiteral ("m_config.transceiver_online ();"),
                           QStringLiteral ("transceiver_session_generation ();"))
               && ordered (requestPath,
                           QStringLiteral ("PttStartPolicy::ready_for_request"),
                           QStringLiteral ("setRig ();"))
               && ordered (requestPath,
                           QStringLiteral ("setRig ();"),
                           QStringLiteral ("transceiver_session_generation () != rigGeneration"))
               && ordered (requestPath,
                           QStringLiteral ("transceiver_session_generation () != rigGeneration"),
                           QStringLiteral ("m_pttStartPolicy.begin"))
               && ordered (requestPath,
                           QStringLiteral ("m_pttStartPolicy.begin"),
                           QStringLiteral ("Q_EMIT m_config.transceiver_ptt (true)")),
           "the request resolves the rig first and rechecks its generation before creating a token and sending PTT");
  require (requestPath.contains (QStringLiteral ("ptt_readback_serial"))
               && mainwindow.contains (QStringLiteral ("ptt_request_readback_baseline"))
               && mainwindow.contains (QStringLiteral ("ptt_request_generation"))
               && mainwindow.contains (QStringLiteral ("ptt_token_active"))
               && mainwindow.contains (QStringLiteral ("ptt_request_pending"))
               && mainwindow.contains (QStringLiteral ("ptt_timer_active")),
           "request diagnostics retain serial, generation, state-generation, and timer/token status");

  auto const rigSessionConfigStart = configuration.indexOf (
    QStringLiteral ("void append_rig_session_config"));
  auto const rigSessionTxDelayStart = configuration.indexOf (
    QStringLiteral ("void append_rig_session_txdelay"), rigSessionConfigStart);
  auto const rigSessionConfig = configuration.mid (
    rigSessionConfigStart, rigSessionTxDelayStart - rigSessionConfigStart);
  require (rigSessionConfigStart >= 0 && rigSessionTxDelayStart > rigSessionConfigStart
               && rigSessionConfig.contains (QStringLiteral ("rig_session_config phase=create"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.rig_name"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.baud"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.data_bits"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.stop_bits"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.handshake"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.ptt_type"))
               && rigSessionConfig.contains (QStringLiteral ("rig_data.split_mode"))
               && !rigSessionConfig.contains (QStringLiteral ("serial_port"))
               && !rigSessionConfig.contains (QStringLiteral ("network_port"))
               && !rigSessionConfig.contains (QStringLiteral ("tci_port"))
               && !rigSessionConfig.contains (QStringLiteral ("usb_port"))
               && !rigSessionConfig.contains (QStringLiteral ("ptt_port")),
           "rig_session_config snapshots only typed safe settings and excludes device endpoints");
  require (ordered (configuration,
                    QStringLiteral ("auto const rig_data = gather_rig_data ();"),
                    QStringLiteral ("append_rig_session_config (rig_data, generation, rig_session_purpose_);"))
               && ordered (configuration,
                           QStringLiteral ("txDelay_ = ui_->sbTxDelay->value ();"),
                           QStringLiteral ("append_rig_session_txdelay (rig_generation_, RigSessionPurpose::runtime,"))
               && configuration.contains (QStringLiteral ("txdelay_ms=%3 accepted=true")),
           "configuration logs immutable rig input at create and effective delay only after acceptance");

  auto const requestRejectStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::rejectFtx1PttRequest"));
  auto const requestRejectEnd = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"), requestRejectStart);
  auto const requestReject = mainwindow.mid (requestRejectStart,
                                              requestRejectEnd - requestRejectStart);
  require (requestRejectStart >= 0 && requestRejectEnd > requestRejectStart
               && requestReject.contains (QStringLiteral ("m_ftx1ReadinessRejectedBatchId = m_autoSeqDecodeBatchId"))
               && requestReject.contains (QStringLiteral ("m_autoSeqInitialCallStall.is_owned_call"))
               && requestReject.contains (QStringLiteral ("m_autoSeqRecovery.pending ()"))
               && requestReject.contains (QStringLiteral ("m_autoSeqInitialCallOperatorHold"))
               && ordered (requestReject,
                           QStringLiteral ("m_autoSeqInternalEnableTxChange = true"),
                           QStringLiteral ("enableTx_mode (false)"))
               && ordered (requestReject,
                           QStringLiteral ("if (m_tune)"),
                           QStringLiteral ("on_tuneButton_clicked (false)")),
           "not-ready refusal persists by decode batch, and only internally disables an owned initial target or stops Tune");

  auto const pendingCancelStart = mainwindow.indexOf (
    QStringLiteral ("bool MainWindow::cancelPendingPttStart"));
  auto const pendingCancelEnd = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::rejectFtx1PttRequest"), pendingCancelStart);
  auto const pendingCancel = mainwindow.mid (pendingCancelStart,
                                              pendingCancelEnd - pendingCancelStart);
  auto const clearDxStart = mainwindow.indexOf (QStringLiteral ("void MainWindow::clearDX (QString reason)"));
  auto const clearDxEnd = mainwindow.indexOf (QStringLiteral ("void MainWindow::clearDXfields"), clearDxStart);
  auto const clearDx = mainwindow.mid (clearDxStart, clearDxEnd - clearDxStart);
  auto const manualDecodeStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::on_DecodeButton_clicked"));
  auto const manualDecodeEnd = mainwindow.indexOf (QStringLiteral ("void MainWindow::freezeDecode"),
                                                   manualDecodeStart);
  auto const manualDecode = mainwindow.mid (manualDecodeStart,
                                            manualDecodeEnd - manualDecodeStart);
  require (pendingCancelStart >= 0 && pendingCancelEnd > pendingCancelStart
               && pendingCancel.contains (QStringLiteral ("ptt1Timer.stop ()"))
               && pendingCancel.contains (QStringLiteral ("m_pttStartPolicy.cancel ()"))
               && pendingCancel.contains (QStringLiteral ("m_tx_when_ready = false"))
               && pendingCancel.contains (QStringLiteral ("m_haltTrans = true"))
               && clearDxStart >= 0 && ordered (clearDx,
                                                QStringLiteral ("cancelPendingPttStart"),
                                                QStringLiteral ("clearDXfields"))
               && manualDecodeStart >= 0 && ordered (manualDecode,
                                                     QStringLiteral ("cancelPendingPttStart"),
                                                     QStringLiteral ("m_manualDecode=true"))
               && manualDecode.contains (QStringLiteral ("manual_decode_takeover"))
               && manualDecode.contains (QStringLiteral ("m_ftx1ReadinessRejectedBatchId")),
           "Clear DX and manual decode cancel an unconsumed request; FTX-1 CAT manual takeover cannot retry in the same batch");

  auto const freshRearmStart = mainwindow.indexOf (
    QStringLiteral ("bool const freshFtx1AutoDecision"));
  auto const freshRearmEnd = mainwindow.indexOf (
    QStringLiteral ("if (rx > 0 && rx != ui->RxFreqSpinBox->value ())"), freshRearmStart);
  auto const freshRearm = mainwindow.mid (freshRearmStart, freshRearmEnd - freshRearmStart);
  require (freshRearmStart >= 0 && freshRearmEnd > freshRearmStart
               && freshRearm.contains (QStringLiteral ("PttStartPolicy::fresh_auto_decision"))
               && freshRearm.contains (QStringLiteral ("PttStartPolicy::may_rearm_initial_owner"))
               && freshRearm.contains (QStringLiteral ("readinessSameOwner"))
               && freshRearm.contains (QStringLiteral ("readinessInitialCandidate"))
               && freshRearm.contains (QStringLiteral ("m_autoSeqRecovery.pending ()"))
               && freshRearm.contains (QStringLiteral ("m_autoSeqInitialCallOperatorHold"))
               && ordered (freshRearm,
                           QStringLiteral ("m_ftx1PttReadinessRejected = false"),
                           QStringLiteral ("enableTx_mode (true)")),
           "only a fresh live AutoSeq decision for the same initial RCQ/RFIN owner can re-arm the internal disable");

  auto const enableTxStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::on_enableTxButton_clicked (bool checked)"));
  auto const tuneStartForIntent = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::on_tuneButton_clicked (bool checked)"));
  require (enableTxStart >= 0 && tuneStartForIntent > enableTxStart
               && mainwindow.mid (enableTxStart, tuneStartForIntent - enableTxStart)
                    .contains (QStringLiteral ("if (checked && !m_autoSeqInternalEnableTxChange)"))
               && mainwindow.mid (enableTxStart, tuneStartForIntent - enableTxStart)
                    .contains (QStringLiteral ("m_ftx1PttReadinessRejected = false"))
                && mainwindow.mid (enableTxStart, tuneStartForIntent - enableTxStart)
                     .contains (QStringLiteral ("PttStartPolicy::should_cancel_pending_audio"))
                && mainwindow.mid (enableTxStart, tuneStartForIntent - enableTxStart)
                     .contains (QStringLiteral ("cancelPendingPttStart (QStringLiteral (\"enable_tx_disabled_pending_audio\"))"))
               && mainwindow.mid (tuneStartForIntent,
                                  mainwindow.indexOf (QStringLiteral ("void MainWindow::stop_tuning ()"),
                                                      tuneStartForIntent) - tuneStartForIntent)
                    .contains (QStringLiteral ("m_ftx1PttReadinessRejected = false")),
           "explicit EnableTx-on and Tune-on clear refusal; internal EnableTx changes do not");

  auto const guiSlotBoundary = mainwindow.indexOf (
    QStringLiteral ("PttStartPolicy::should_cancel_request_at_slot_end"));
  auto const guiSlotCalc = mainwindow.indexOf (QStringLiteral ("if(m_tune) m_bTxTime=true"));
  auto const guiTxFlow = mainwindow.indexOf (QStringLiteral ("if(m_transmitting or m_enableTx or m_tune)"),
                                             guiSlotBoundary);
  auto const dataSinkStart = mainwindow.indexOf (QStringLiteral ("void MainWindow::dataSink(qint64 frames)"));
  auto const dataSinkEnd = mainwindow.indexOf (QStringLiteral ("void MainWindow::"), dataSinkStart + 1);
  auto const dataSink = mainwindow.mid (dataSinkStart, dataSinkEnd - dataSinkStart);
  require (guiSlotCalc >= 0 && guiSlotBoundary > guiSlotCalc && guiTxFlow > guiSlotBoundary
               && ordered (mainwindow.mid (guiSlotCalc, guiTxFlow - guiSlotCalc),
                           QStringLiteral ("PttStartPolicy::should_cancel_request_at_slot_end"),
                           QStringLiteral ("m_haltTrans = true"))
               && !dataSink.contains (QStringLiteral ("should_cancel_request_at_slot_end"))
               && !dataSink.contains (QStringLiteral ("m_haltTrans")),
           "slot-end PTT/audio cancellation and lower-PTT request run from guiUpdate, not audio dataSink callbacks");

  auto const tuneStopStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::on_tuneButton_clicked (bool checked)"));
  auto const tuneStopEnd = mainwindow.indexOf (QStringLiteral ("void MainWindow::stop_tuning ()"),
                                               tuneStopStart);
  auto const tuneStop = mainwindow.mid (tuneStopStart, tuneStopEnd - tuneStopStart);
  require (ordered (tuneStop,
                    QStringLiteral ("if(!checked)"),
                    QStringLiteral ("ptt1Timer.stop ();"))
                && ordered (tuneStop,
                            QStringLiteral ("ptt1Timer.stop ();"),
                            QStringLiteral ("cancelPttStartAndRevokeAdmission"))
               && tuneStop.contains (QStringLiteral ("m_tuneStopPending = true;"))
               && mainwindow.contains (QStringLiteral ("m_tuneStopPending))")),
           "Tune-off immediately cancels delayed audio and closes request admission during the existing shutdown delay");

  auto const audioStart = mainwindow.indexOf (QStringLiteral ("void MainWindow::startTx2()"));
  auto const audioStartEnd = mainwindow.indexOf (QStringLiteral ("void MainWindow::stopTx2()"),
                                                 audioStart);
  auto const audioStartMethod = mainwindow.mid (audioStart, audioStartEnd - audioStart);
  require (ordered (audioStartMethod,
                    QStringLiteral ("(m_manualDecode && !m_tune)"),
                    QStringLiteral ("consume_audio ("))
               && ordered (audioStartMethod,
                    QStringLiteral ("consume_audio ("),
                    QStringLiteral ("m_tx_when_ready = false;"))
                && ordered (audioStartMethod,
                           QStringLiteral ("consume_audio ("),
                           QStringLiteral ("m_modulator->setSpread"))
               && audioStartMethod.contains (QStringLiteral ("m_rigState.ptt_readback_serial ()")),
           "the timer consumes a current token only with current-generation, online, known PTT readback");

  auto const payloadSelectorStart = mainwindow.indexOf (
    QStringLiteral ("QByteArray MainWindow::selectedTxPayloadBytes () const"));
  auto const requestContextStart = mainwindow.indexOf (
    QStringLiteral ("PttStartPolicy::RequestContext MainWindow::currentPttRequestContext () const"));
  require (payloadSelectorStart >= 0 && requestContextStart >= 0
               && txPayload.contains (QStringLiteral ("inline std::string select ("))
               && mainwindow.contains (QStringLiteral ("PttTxPayload::select (m_ntx, messages)"))
               && mainwindow.contains (QStringLiteral ("PttTxPayload::identity ("))
               && mainwindow.contains (QStringLiteral ("context.tx_payload_identity"))
               && mainwindow.contains (QStringLiteral ("auto const selectedPayload = selectedTxPayloadBytes ();"))
               && mainwindow.contains (QStringLiteral ("else { ba=selectedPayload; }"))
               && pttStartPolicy.contains (QStringLiteral (
                    "requested.tx_payload_identity == current.tx_payload_identity")),
           "the GUI encoder and request context share the production TX1-TX8 selector and effective payload identity");
  require (ordered (audioStartMethod,
                    QStringLiteral ("cancelPendingPttStartIfContextChanged"),
                    QStringLiteral ("consume_audio ("))
               && mainwindow.contains (QStringLiteral ("if (m_mode.left (4) != \"WSPR\")")),
           "a changed selected payload is cancelled before token consumption while WSPR stays on its separate generator path");

  auto const stateUpdateStart = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::handle_transceiver_update (Transceiver::TransceiverState const& s)"));
  auto const stateUpdateEnd = mainwindow.indexOf (
    QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"), stateUpdateStart);
  auto const stateUpdate = mainwindow.mid (stateUpdateStart, stateUpdateEnd - stateUpdateStart);
  require (pttObservationPolicy.contains (QStringLiteral (
               "readback_serial <= previous.serial"))
               && pttObservationPolicy.contains (QStringLiteral (
                    "Action::ignored_stale_serial"))
               && pttObservationPolicy.contains (QStringLiteral (
                    "Action::accepted_real_get"))
               && ordered (pttObservationPolicy,
                           QStringLiteral ("if (!online)"),
                           QStringLiteral ("if (readback_required)"))
               && ordered (pttObservationPolicy,
                           QStringLiteral ("if (readback_required)"),
                           QStringLiteral ("Action::accepted_legacy_state"))
               && pttObservationPolicy.contains (QStringLiteral (
                    "Snapshot {true, ptt_on, now_ms, readback_serial}"))
               && pttObservationPolicy.contains (QStringLiteral (
                    "Snapshot {true, ptt_on, now_ms, previous.serial}")),
           "strict GET observations require a newer serial; current online legacy observations may refresh time");
  require (ordered (stateUpdate,
                    QStringLiteral ("AutoSeqPttObservationPolicy::observe ("),
                    QStringLiteral ("if (pttStateObservation.accepted ())"))
               && ordered (stateUpdate,
                           QStringLiteral ("if (pttStateObservation.accepted ())"),
                           QStringLiteral ("m_autoSeqPttReadbackAtUtcMs = QDateTime::currentDateTimeUtc"))
               && stateUpdate.contains (QStringLiteral (
                    "pttReadbackAdvanced = pttStateObservation.real_get_advanced ()"))
               && stateUpdate.contains (QStringLiteral (
                    "m_rigStateGeneration = rigGeneration")),
           "only an accepted current-generation observation refreshes readback time and state generation");
  require (ordered (stateUpdate,
                    QStringLiteral ("if (pttJustTurnedOn && m_autoSeqInitialCallStall.active ()"),
                    QStringLiteral ("m_autoSeqInitialCallStall.observe_actual_progress ("))
               && stateUpdate.contains (QStringLiteral (
                    "&& (pttReadbackAdvanced || acceptedLegacyObservation)"))
               && stateUpdate.contains (QStringLiteral (
                    "acceptedLegacyObservation = pttStateObservation.action")),
           "actual AutoSeq progress requires the actual PTT-on edge plus strict GET or accepted legacy observation");

  auto const pollingStateGate = polling.indexOf (QStringLiteral ("bool PollingTransceiver::do_pre_update"));
  auto const pollingTimeout = polling.indexOf (QStringLiteral ("void PollingTransceiver::handle_timeout"));
  require (pollingStateGate >= 0 && pollingTimeout > pollingStateGate
               && polling.mid (pollingStateGate, pollingTimeout - pollingStateGate)
                    .contains (QStringLiteral ("same_control_state (next_state_)"))
               && transceiverState.contains (QStringLiteral ("ptt_readback_serial_ = rhs.ptt_readback_serial_ = 0")),
           "PTT readback notifications are visible without changing the polling command-stability comparator");
  return 0;
}
