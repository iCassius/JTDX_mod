#include "JtdxWebControl.hpp"

#include <QCoreApplication>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace
{
  using Control = JtdxWebControl;

  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  Control::ObservedState safe_state (quint64 revision = 1)
  {
    Control::ObservedState state;
    state.safety.known = true;
    state.safety.business_state_known = true;
    state.safety.rig_online = true;
    state.safety.monitoring = true;
    state.state_revision = revision;
    state.frequency_generation = 4;
    state.frequency_known = true;
    state.actual_frequency_hz = 14073000;
    state.dx_generation = 8;
    return state;
  }

  Control::Request frequency_request (Control const& control, QString id, qint64 frequency,
                                      quint64 revision = 1)
  {
    Control::Request request;
    request.request_id = std::move (id);
    request.operation = Control::Operation::Frequency;
    request.server_epoch = control.server_epoch ();
    request.state_revision = revision;
    request.frequency_hz = frequency;
    return request;
  }

  void bind_fixture_epoch (Control& control)
  {
    QString const epoch = control.server_epoch ();
    control.bind_server_epoch (epoch);
  }

  Control::Result submit_radio_action (QString action, bool value,
                                       Control::SafetySnapshot safety, QString request_id)
  {
    Control control {100};
    bind_fixture_epoch (control);
    control.set_clock_for_test (1000);
    auto observed = safe_state (1);
    observed.safety = safety;
    observed.radio_state_known = true;
    control.set_observed_state (observed);
    control.set_business_dispatcher ([] (Control::Dispatch const&) {});
    Control::Request request;
    request.request_id = std::move (request_id);
    request.operation = Control::Operation::Radio;
    request.server_epoch = control.server_epoch ();
    request.state_revision = 1;
    request.radio_action = std::move (action);
    request.radio_value = value;
    request.radio_index = 1;
    request.radio_text = QStringLiteral ("CQ W1ABC FN31");
    if (request.radio_action == QStringLiteral ("log-qso-confirm"))
      request.radio_qso = QJsonObject {{QStringLiteral ("call"), QStringLiteral ("W1ABC")}};
    return control.submit (request);
  }

  Control::Result submit_operation (Control::Operation operation, Control::SafetySnapshot safety,
                                    QString request_id)
  {
    Control control {100};
    bind_fixture_epoch (control);
    control.set_clock_for_test (1000);
    auto observed = safe_state (1);
    observed.safety = safety;
    observed.radio_state_known = true;
    control.set_observed_state (observed);
    control.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
    control.set_select_dx_dispatcher ([] (Control::Dispatch const&) {});
    control.set_business_dispatcher ([] (Control::Dispatch const&) {});
    Control::Request request;
    request.request_id = std::move (request_id);
    request.operation = operation;
    request.server_epoch = control.server_epoch ();
    request.state_revision = 1;
    request.frequency_hz = 14074000;
    request.dx_call = QStringLiteral ("W1ABC");
    request.dx_grid = QStringLiteral ("FN31");
    return control.submit (request);
  }
}

int main ()
{
  Control control {100};
  bind_fixture_epoch (control);
  control.set_clock_for_test (1000);
  control.set_observed_state (safe_state (1));

  Control::ObservedState unsafe = safe_state (1);
  unsafe.safety.known = false;
  control.set_observed_state (unsafe);
  auto rejected = control.submit (frequency_request (control, QStringLiteral ("unknown"), 14074000));
  check (rejected.status == Control::Status::Rejected && rejected.reason == QStringLiteral ("safety_unknown"),
         "unknown safety fails closed");
  check (rejected.received_ms == 1000 && rejected.snapshot.state_revision == 1,
         "rejected result preserves bounded timestamp and cached observation");
  control.set_observed_state (safe_state (1));
  auto aged_but_known = safe_state (1);
  Control aged_state_control {100};
  bind_fixture_epoch (aged_state_control);
  aged_state_control.set_clock_for_test (1000);
  aged_state_control.set_observed_state (aged_but_known);
  aged_state_control.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
  check (aged_state_control.submit (frequency_request (aged_state_control,
              QStringLiteral ("aged-known"), 14074000)).status == Control::Status::Pending,
         "known native safety state remains admissible without a Web snapshot-age gate");
  control.set_observed_state (safe_state (1));

  Control startup_gate {100};
  bind_fixture_epoch (startup_gate);
  startup_gate.set_clock_for_test (1000);
  auto startup_state = safe_state (1);
  startup_state.safety.start2 = true;
  startup_gate.set_observed_state (startup_state);
  auto startup_rejected = startup_gate.submit (
      frequency_request (startup_gate, QStringLiteral ("startup"), 14074000));
  check (startup_rejected.status == Control::Status::Rejected
         && startup_rejected.reason == QStringLiteral ("startup_pending"),
         "first rig observation startup gate is reported separately from active TX");

  Control tx_path_gate {100};
  bind_fixture_epoch (tx_path_gate);
  tx_path_gate.set_clock_for_test (1000);
  auto tx_path_state = safe_state (1);
  tx_path_state.safety.auto_tx = true;
  tx_path_gate.set_observed_state (tx_path_state);
  auto tx_path_rejected = tx_path_gate.submit (
      frequency_request (tx_path_gate, QStringLiteral ("tx-path"), 14074000));
  check (tx_path_rejected.status == Control::Status::Rejected
         && tx_path_rejected.reason == QStringLiteral ("tx_path_active"),
         "active AutoTx path remains rejected with its existing reason");

  QStringList const local_radio_actions {
    QStringLiteral ("log-qso"), QStringLiteral ("log-qso-confirm"),
    QStringLiteral ("log-qso-cancel"), QStringLiteral ("clear-windows"),
    QStringLiteral ("sync"), QStringLiteral ("multi-decode"),
    QStringLiteral ("agc-compensation"), QStringLiteral ("narrow"),
    QStringLiteral ("decode"), QStringLiteral ("clear-dx"),
    QStringLiteral ("generate-message"), QStringLiteral ("skip-tx1"),
    QStringLiteral ("select-tx"), QStringLiteral ("set-tx-message")};
  QStringList const safety_conditions {
    QStringLiteral ("unknown"), QStringLiteral ("rig_offline"),
    QStringLiteral ("monitor_off"), QStringLiteral ("transmitting"),
    QStringLiteral ("ptt"), QStringLiteral ("tx_enabled"),
    QStringLiteral ("watchdog"), QStringLiteral ("start2"),
    QStringLiteral ("tune"), QStringLiteral ("auto_tx"),
    QStringLiteral ("iptt"), QStringLiteral ("business_unknown")};
  for (auto const& condition : safety_conditions)
    {
      auto safety = safe_state (1).safety;
      if (condition == QStringLiteral ("unknown")) safety.known = false;
      else if (condition == QStringLiteral ("rig_offline")) safety.rig_online = false;
      else if (condition == QStringLiteral ("monitor_off")) safety.monitoring = false;
      else if (condition == QStringLiteral ("transmitting")) safety.transmitting = true;
      else if (condition == QStringLiteral ("ptt")) safety.ptt = true;
      else if (condition == QStringLiteral ("tx_enabled")) safety.tx_enabled = true;
      else if (condition == QStringLiteral ("watchdog")) safety.watchdog_timeout = true;
      else if (condition == QStringLiteral ("start2")) safety.start2 = true;
      else if (condition == QStringLiteral ("tune")) safety.tune = true;
      else if (condition == QStringLiteral ("auto_tx")) safety.auto_tx = true;
      else if (condition == QStringLiteral ("iptt")) safety.iptt = true;
      else if (condition == QStringLiteral ("business_unknown")) safety.business_state_known = false;

      for (auto const& action : local_radio_actions)
        check (submit_radio_action (action, false, safety, condition + QLatin1Char ('-') + action).status
                   == Control::Status::Pending,
               "non-transmit radio action is not blocked by unrelated TX/startup conditions");
      check (submit_operation (Control::Operation::SelectDx, safety,
                               condition + QStringLiteral ("-select-dx")).status == Control::Status::Pending,
             "selecting a decoded DX only updates the input projection and is not TX-gated");
      check (submit_radio_action (QStringLiteral ("enable-tx"), false, safety,
                                  condition + QStringLiteral ("-disable-tx")).status == Control::Status::Pending,
             "disabling TX remains available in every observed safety condition");
      check (submit_radio_action (QStringLiteral ("stop-tx"), false, safety,
                                  condition + QStringLiteral ("-stop-tx")).status == Control::Status::Pending,
             "Stop Tx remains available in every observed safety condition");
      auto enable_tx = submit_radio_action (QStringLiteral ("enable-tx"), true, safety,
                                            condition + QStringLiteral ("-enable-tx"));
      auto cq = submit_radio_action (QStringLiteral ("cq"), true, safety,
                                     condition + QStringLiteral ("-cq"));
      QString expected_reason = QStringLiteral ("tx_path_active");
      if (condition == QStringLiteral ("unknown")) expected_reason = QStringLiteral ("safety_unknown");
      else if (condition == QStringLiteral ("rig_offline")) expected_reason = QStringLiteral ("rig_offline");
      else if (condition == QStringLiteral ("monitor_off")) expected_reason = QStringLiteral ("monitor_not_active");
      else if (condition == QStringLiteral ("transmitting")) expected_reason = QStringLiteral ("transmitting");
      else if (condition == QStringLiteral ("ptt")) expected_reason = QStringLiteral ("ptt_active");
      else if (condition == QStringLiteral ("tx_enabled")) expected_reason = QStringLiteral ("tx_enabled");
      else if (condition == QStringLiteral ("watchdog")) expected_reason = QStringLiteral ("watchdog_timeout");
      else if (condition == QStringLiteral ("start2")) expected_reason = QStringLiteral ("startup_pending");
      else if (condition == QStringLiteral ("business_unknown")) expected_reason = QStringLiteral ("business_state_unknown");
      auto frequency = submit_operation (Control::Operation::Frequency, safety,
                                         condition + QStringLiteral ("-frequency"));
      auto start_cq = submit_operation (Control::Operation::StartCq, safety,
                                        condition + QStringLiteral ("-start-cq"));
      auto start_auto = submit_operation (Control::Operation::StartAutoCall, safety,
                                          condition + QStringLiteral ("-start-auto"));
      auto stop_business = submit_operation (Control::Operation::StopAutoCall, safety,
                                            condition + QStringLiteral ("-stop-auto"));
      check (enable_tx.status == Control::Status::Rejected && cq.status == Control::Status::Rejected,
             "TX arming and CQ remain behind the native safety-state matrix");
      check (enable_tx.reason == expected_reason && cq.reason == expected_reason,
             "TX-enabling actions report the precise first failing safety condition");
      check (frequency.status == Control::Status::Rejected && start_cq.status == Control::Status::Rejected
                 && start_auto.status == Control::Status::Rejected
                 && frequency.reason == expected_reason && start_cq.reason == expected_reason
                 && start_auto.reason == expected_reason,
             "frequency changes and CQ/AutoSeq starts retain the precise native safety gate");
      check (stop_business.status == Control::Status::Pending,
             "Stop AutoSeq remains available in every observed safety condition");
    }

  Control stop_priority {100};
  bind_fixture_epoch (stop_priority);
  stop_priority.set_clock_for_test (1000);
  stop_priority.set_observed_state (safe_state (1));
  int stop_priority_dispatches = 0;
  stop_priority.set_business_dispatcher ([&] (Control::Dispatch const&) { ++stop_priority_dispatches; });
  Control::Request queued_radio;
  queued_radio.request_id = QStringLiteral ("queued-radio-change");
  queued_radio.operation = Control::Operation::Radio;
  queued_radio.server_epoch = stop_priority.server_epoch ();
  queued_radio.state_revision = 1;
  queued_radio.radio_action = QStringLiteral ("set-tx-message");
  queued_radio.radio_index = 1;
  queued_radio.radio_text = QStringLiteral ("CQ W1ABC FN31");
  check (stop_priority.submit (queued_radio).status == Control::Status::Pending,
         "TX message editing request can enter the existing MainWindow route");
  auto priority_stop_request = queued_radio;
  priority_stop_request.request_id = QStringLiteral ("priority-stop");
  priority_stop_request.radio_action = QStringLiteral ("stop-tx");
  auto const stop_result = stop_priority.submit (priority_stop_request);
  check (stop_result.status == Control::Status::Pending
             && stop_priority.result (QStringLiteral ("queued-radio-change")).reason
                    == QStringLiteral ("superseded_by_stop")
             && stop_priority_dispatches == 2,
         "Stop Tx supersedes queued radio work and is dispatched first");

  Control unknown_stop {10};
  bind_fixture_epoch (unknown_stop);
  unknown_stop.set_clock_for_test (1000);
  auto unknown_stop_baseline = safe_state (1);
  unknown_stop_baseline.radio_state_known = true;
  unknown_stop.set_observed_state (unknown_stop_baseline);
  Control::Dispatch dispatched_unknown_radio;
  unknown_stop.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      dispatched_unknown_radio = dispatch;
    });
  auto unknown_radio = queued_radio;
  unknown_radio.request_id = QStringLiteral ("unknown-radio-before-stop");
  unknown_radio.server_epoch = unknown_stop.server_epoch ();
  check (unknown_stop.submit (unknown_radio).status == Control::Status::Pending,
         "radio operation can be pending before its bounded timeout");
  Control::Dispatch prepared_unknown_radio;
  check (unknown_stop.prepare_dispatch (dispatched_unknown_radio.request_id,
                                        dispatched_unknown_radio.server_epoch,
                                        &prepared_unknown_radio)
             && unknown_stop.begin_dispatch (prepared_unknown_radio),
         "radio operation begins before the unconfirmed-result timeout");
  unknown_stop.advance_clock_for_test (11);
  unknown_stop.expire ();
  Control::Dispatch stop_after_unknown_dispatch;
  unknown_stop.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      stop_after_unknown_dispatch = dispatch;
    });
  auto unknown_stop_request = unknown_radio;
  unknown_stop_request.request_id = QStringLiteral ("stop-after-unknown-radio");
  unknown_stop_request.radio_action = QStringLiteral ("stop-tx");
  auto const stop_under_latch = unknown_stop.submit (unknown_stop_request);
  Control::Dispatch prepared_stop_after_unknown;
  check (unknown_stop.prepare_dispatch (stop_after_unknown_dispatch.request_id,
                                        stop_after_unknown_dispatch.server_epoch,
                                        &prepared_stop_after_unknown)
             && unknown_stop.begin_dispatch (prepared_stop_after_unknown),
         "Stop Tx prepares and begins while preserving the old-result latch");
  auto stop_feedback = safe_state (2);
  stop_feedback.radio_state_known = true;
  check (unknown_stop.feedback_radio (prepared_stop_after_unknown.request_id,
                                      prepared_stop_after_unknown.server_epoch, 2, stop_feedback),
         "Stop Tx can be confirmed by radio-state readback without touching the prior latch");
  auto blocked_local_under_latch = unknown_stop_request;
  blocked_local_under_latch.request_id = QStringLiteral ("blocked-after-stop");
  blocked_local_under_latch.radio_action = QStringLiteral ("clear-windows");
  auto const ordinary_under_latch = unknown_stop.submit (blocked_local_under_latch);
  check (stop_under_latch.status == Control::Status::Pending
             && ordinary_under_latch.status == Control::Status::Rejected
             && ordinary_under_latch.reason == QStringLiteral ("unconfirmed_feedback"),
         "Stop Tx remains available under the unknown-result latch without unlocking ordinary work");

  int dispatch_count = 0;
  control.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    ++dispatch_count;
    check (dispatch.server_epoch == control.server_epoch (), "dispatch carries epoch");
    Control::Dispatch prepared;
    check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "queued dispatch is prepared at execution time");
    check (control.begin_dispatch (prepared), "prepared dispatch begins once");
    check (control.feedback_frequency (dispatch.request_id, dispatch.server_epoch,
                                       prepared.expected_generation + 1, prepared.frequency_hz, 2),
           "synchronous feedback is accepted");
  });
  auto completed = control.submit (frequency_request (control, QStringLiteral ("sync"), 14074000));
  check (completed.status == Control::Status::Completed && completed.reason == QStringLiteral ("feedback_matched"),
         "synchronous dispatch feedback completes");
  check (dispatch_count == 1, "synchronous dispatch occurs once");
  check (control.operation_revision () > 0 && control.operation_results ().size () == 1,
         "operation revision and bounded result copy expose the completed record");
  check (control.operation_results ().constFirst ().request_id == QStringLiteral ("sync"),
         "operation result copy preserves request id");
  auto duplicate = control.submit (frequency_request (control, QStringLiteral ("sync"), 14074000));
  check (duplicate.status == Control::Status::Completed && dispatch_count == 1,
         "same id and payload is idempotent");
  auto conflict = control.submit (frequency_request (control, QStringLiteral ("sync"), 14075000));
  check (conflict.status == Control::Status::Rejected && conflict.http_status == 409
         && conflict.reason == QStringLiteral ("request_id_conflict") && dispatch_count == 1,
         "same id with different payload is a 409 without dispatch");

  Control::Dispatch queued_dispatch;
  control.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    queued_dispatch = dispatch;
  });
  auto pending = control.submit (frequency_request (control, QStringLiteral ("pending"), 14075000));
  check (pending.status == Control::Status::Pending && control.has_pending (), "request enters pending");
  check (!control.feedback_frequency (QStringLiteral ("pending"), control.server_epoch (), 5, 14075000, 2),
         "feedback cannot complete before queued dispatch begins");
  check (!control.begin_dispatch (queued_dispatch), "dispatch cannot begin without prepare");
  Control::Dispatch pending_dispatch;
  check (control.prepare_dispatch (QStringLiteral ("pending"), control.server_epoch (), &pending_dispatch),
         "pending request prepares from latest observation");
  check (control.begin_dispatch (pending_dispatch), "pending request begins dispatch");
  check (!control.begin_dispatch (pending_dispatch), "same dispatch cannot be consumed twice");
  check (!control.feedback_frequency (QStringLiteral ("pending"), control.server_epoch (), 4, 14075000, 99),
         "old generation cannot complete despite newer global revision");
  check (!control.feedback_frequency (QStringLiteral ("other"), control.server_epoch (), 5, 14075000, 2),
         "feedback for another request cannot complete pending request");
  check (control.feedback_frequency (QStringLiteral ("pending"), control.server_epoch (), 5, 14075000, 2),
         "matching new generation completes pending request");

  Control begin_guard {100};
  bind_fixture_epoch (begin_guard);
  begin_guard.set_clock_for_test (0);
  begin_guard.set_observed_state (safe_state (1));
  Control::Dispatch begin_candidate;
  begin_guard.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    begin_candidate = dispatch;
  });
  check (begin_guard.submit (frequency_request (begin_guard, QStringLiteral ("begin-guard"), 14074000)).status
         == Control::Status::Pending, "begin guard request enters pending");
  check (begin_guard.prepare_dispatch (begin_candidate.request_id, begin_candidate.server_epoch,
                                       &begin_candidate), "begin guard prepares request");
  auto newer_observation = safe_state (1);
  newer_observation.frequency_generation = begin_candidate.expected_generation + 1;
  begin_guard.set_observed_state (newer_observation);
  check (!begin_guard.begin_dispatch (begin_candidate),
         "generation change between prepare and begin fails closed");
  check (begin_guard.result (QStringLiteral ("begin-guard")).status == Control::Status::Pending,
         "generation mismatch leaves request retryable before deadline");

  auto select = Control::Request {};
  select.request_id = QStringLiteral ("dx-1");
  select.operation = Control::Operation::SelectDx;
  select.server_epoch = control.server_epoch ();
  select.state_revision = 1;
  select.dx_call = QStringLiteral (" ja2lcp ");
  select.dx_grid = QStringLiteral ("pm95");
  bool select_dispatched = false;
  control.set_select_dx_dispatcher ([&] (Control::Dispatch const& dispatch) {
    select_dispatched = true;
    check (dispatch.dx_call == QStringLiteral ("JA2LCP") && dispatch.dx_grid == QStringLiteral ("PM95"),
           "DX target is normalized at envelope boundary");
    Control::Dispatch prepared;
    check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "DX dispatch prepares at execution time");
    check (control.begin_dispatch (prepared), "DX dispatch begins once");
  });
  auto select_pending = control.submit (select);
  check (select_pending.status == Control::Status::Pending && select_dispatched, "DX enters pending through dispatch");
  check (!control.feedback_select_dx (QStringLiteral ("dx-1"), control.server_epoch (), 8,
                                     QStringLiteral ("JA2LCP"), QStringLiteral ("PM95"), 2),
         "equal DX generation is not completion");
  check (control.feedback_select_dx (QStringLiteral ("dx-1"), control.server_epoch (), 9,
                                    QStringLiteral ("JA2LCP"), QStringLiteral ("PM95"), 2),
         "DX independent feedback generation completes");

  Control::Request start_cq;
  start_cq.request_id = QStringLiteral ("cq-1");
  start_cq.operation = Control::Operation::StartCq;
  start_cq.server_epoch = control.server_epoch ();
  start_cq.state_revision = 1;
  bool business_dispatched = false;
  control.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
    business_dispatched = true;
    Control::Dispatch prepared;
    check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "CQ dispatch prepares at execution time");
    check (control.begin_dispatch (prepared), "CQ dispatch begins once");
  });
  auto const cq_pending = control.submit (start_cq);
  check (cq_pending.status == Control::Status::Pending && business_dispatched,
         "CQ command enters pending through the business dispatcher");
  auto business_observation = safe_state (3);
  business_observation.business_generation = 1;
  business_observation.business_state_known = true;
  business_observation.cq_state = QStringLiteral ("armed");
  business_observation.state_revision = 3;
  control.set_observed_state (business_observation);
  check (control.feedback_business (QStringLiteral ("cq-1"), control.server_epoch (), 1,
                                    QStringLiteral ("armed"), false, 3),
         "CQ command requires a later matching business-state readback");
  control.set_observed_state (safe_state (1));

  Control radio_control {100};
  bind_fixture_epoch (radio_control);
  radio_control.set_clock_for_test (0);
  auto radio_observation = safe_state (1);
  radio_observation.radio_state_known = true;
  radio_observation.radio_qso_generation = 0;
  radio_control.set_observed_state (radio_observation);
  radio_control.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
    Control::Dispatch prepared;
    check (radio_control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "radio dispatch prepares at execution time");
    check (radio_control.begin_dispatch (prepared), "radio dispatch begins exactly once");
  });
  Control::Request radio_open;
  radio_open.request_id = QStringLiteral ("radio-open");
  radio_open.operation = Control::Operation::Radio;
  radio_open.server_epoch = radio_control.server_epoch ();
  radio_open.state_revision = 1;
  radio_open.radio_action = QStringLiteral ("log-qso");
  check (radio_control.submit (radio_open).status == Control::Status::Pending,
         "Web QSO open enters pending without writing a record");
  radio_observation.state_revision = 2;
  radio_observation.radio_log_dialog_open = true;
  radio_observation.radio_qso_draft = QJsonObject {{QStringLiteral ("call"), QStringLiteral ("K1ABC")}};
  radio_observation.radio_qso_generation = 1;
  radio_control.set_observed_state (radio_observation);
  check (radio_control.feedback_radio (QStringLiteral ("radio-open"), radio_control.server_epoch (), 2,
                                       radio_observation)
         && radio_control.result (QStringLiteral ("radio-open")).status == Control::Status::Completed,
         "Web QSO open completes only after draft readback");
  Control::Request radio_confirm = radio_open;
  radio_confirm.request_id = QStringLiteral ("radio-confirm");
  radio_confirm.state_revision = 2;
  radio_confirm.radio_action = QStringLiteral ("log-qso-confirm");
  radio_confirm.radio_qso = radio_observation.radio_qso_draft;
  check (radio_control.submit (radio_confirm).status == Control::Status::Pending,
         "Web QSO confirm enters pending with a bounded draft");
  radio_observation.state_revision = 3;
  radio_observation.radio_log_dialog_open = false;
  radio_observation.radio_qso_draft = {};
  radio_observation.radio_qso_generation = 2;
  radio_control.set_observed_state (radio_observation);
  check (radio_control.feedback_radio (QStringLiteral ("radio-confirm"), radio_control.server_epoch (), 3,
                                       radio_observation),
         "Web QSO confirm requires closed draft and a later generation");
  auto duplicate_radio = radio_control.submit (radio_confirm);
  check (duplicate_radio.status == Control::Status::Completed,
         "Web QSO confirm is idempotent by request id");

  Control stop_control {100};
  bind_fixture_epoch (stop_control);
  stop_control.set_clock_for_test (0);
  auto active = safe_state (10);
  active.safety.tx_enabled = true;
  active.safety.transmitting = true;
  active.safety.ptt = true;
  active.safety.watchdog_timeout = true;
  active.business_generation = 4;
  active.auto_sequence_enabled = true;
  active.cq_state = QStringLiteral ("transmitting");
  stop_control.set_observed_state (active);
  Control::Request stop_request;
  stop_request.request_id = QStringLiteral ("stop-active");
  stop_request.operation = Control::Operation::StopAutoCall;
  stop_request.server_epoch = stop_control.server_epoch ();
  stop_request.state_revision = active.state_revision;
  Control::Dispatch stop_dispatch;
  stop_control.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      stop_dispatch = dispatch;
      Control::Dispatch prepared;
      check (stop_control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "stop remains dispatchable while TX/PTT is active");
      check (stop_control.begin_dispatch (prepared), "active stop begins through the existing stop path");
    });
  check (stop_control.submit (stop_request).status == Control::Status::Pending,
         "stop has priority over active TX/PTT safety flags");
  check (stop_control.feedback_business (stop_dispatch.request_id, stop_dispatch.server_epoch,
                                        active.business_generation + 1, QStringLiteral ("idle"), false,
                                        active.state_revision + 1),
         "stop completes only after the disabled AutoSeq/idle business readback");
  check (stop_control.result (stop_request.request_id).reason == QStringLiteral ("automation_stopped"),
         "stop result names the quiesced automation state");

  auto busy_a = frequency_request (control, QStringLiteral ("busy-a"), 14076000);
  auto busy_b = frequency_request (control, QStringLiteral ("busy-b"), 14077000);
  control.set_frequency_dispatcher ({ });
  check (control.submit (busy_a).status == Control::Status::Failed, "first missing dispatch is recorded as failed");
  // Re-enable a pending operation for the busy check.
  control.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
  check (control.submit (busy_b).status == Control::Status::Pending, "second request enters pending");
  auto busy_c = frequency_request (control, QStringLiteral ("busy-c"), 14078000);
  check (control.submit (busy_c).reason == QStringLiteral ("busy"), "different in-flight request is rejected busy");
  control.shutdown ();
  check (!control.feedback_frequency (QStringLiteral ("busy-b"), control.server_epoch (), 6, 14077000, 2),
         "shutdown invalidates feedback");
  check (control.result (QStringLiteral ("busy-b")).reason == QStringLiteral ("shutdown"), "shutdown records rejection");
  check (control.submit (frequency_request (control, QStringLiteral ("after-shutdown"), 14079000)).reason
         == QStringLiteral ("shutdown"), "shutdown rejects later requests");

  Control queued_timeout {100};
  bind_fixture_epoch (queued_timeout);
  queued_timeout.set_clock_for_test (10);
  queued_timeout.set_observed_state (safe_state (1));
  Control::Dispatch old_queued_dispatch;
  queued_timeout.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    if (old_queued_dispatch.request_id.isEmpty ()) old_queued_dispatch = dispatch;
  });
  auto queued_timeout_request = frequency_request (queued_timeout, QStringLiteral ("queued-timeout"), 14074000);
  check (queued_timeout.submit (queued_timeout_request).status == Control::Status::Pending,
         "queued request enters pending before execution");
  queued_timeout.advance_clock_for_test (100);
  check (queued_timeout.expire (), "queued request reaches deadline");
  check (queued_timeout.result (QStringLiteral ("queued-timeout")).status == Control::Status::Timeout,
         "queued request records timeout");
  Control::Dispatch expired_dispatch;
  auto newer_pending = queued_timeout.submit (
      frequency_request (queued_timeout, QStringLiteral ("newer-pending"), 14075000));
  check (newer_pending.status == Control::Status::Pending && queued_timeout.has_pending (),
         "new request can remain pending after queue-only timeout");
  check (!newer_pending.reason.contains (QStringLiteral ("unconfirmed_feedback")),
         "queue-only timeout does not assert hardware latch");
  check (!queued_timeout.prepare_dispatch (old_queued_dispatch.request_id,
                                           old_queued_dispatch.server_epoch, &expired_dispatch),
         "expired old queue cannot execute while newer request is pending");

  Control timeout {100};
  bind_fixture_epoch (timeout);
  timeout.set_clock_for_test (10);
  timeout.set_observed_state (safe_state (1));
  timeout.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    Control::Dispatch prepared;
    check (timeout.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "timeout request prepares before execution");
    check (timeout.begin_dispatch (prepared), "timeout request begins execution");
  });
  auto timeout_request = frequency_request (timeout, QStringLiteral ("timeout"), 14074000);
  check (timeout.submit (timeout_request).status == Control::Status::Pending, "timeout request pending");
  timeout.advance_clock_for_test (100);
  check (!timeout.feedback_frequency (QStringLiteral ("timeout"), timeout.server_epoch (), 5, 14074000, 2),
         "feedback at deadline cannot complete");
  check (timeout.result (QStringLiteral ("timeout")).status == Control::Status::Timeout,
         "deadline produces timeout before late feedback");
  check (!timeout.feedback_frequency (QStringLiteral ("timeout"), timeout.server_epoch (), 6, 14074000, 3),
         "late feedback cannot revive timed-out operation");
  check (timeout.result (QStringLiteral ("timeout")).reason.contains (QStringLiteral ("late_feedback_unknown")),
          "late feedback is explicitly marked unknown");
  auto timeout_duplicate = timeout.submit (timeout_request);
  check (timeout_duplicate.status == Control::Status::Timeout
         && timeout_duplicate.reason.contains (QStringLiteral ("feedback_timeout")),
         "same timed-out id and payload returns original timeout result");
  check (timeout.submit (frequency_request (timeout, QStringLiteral ("after-timeout"), 14074000)).reason
         == QStringLiteral ("unconfirmed_feedback"),
         "timeout holds an unconfirmed latch against a new request");
  check (timeout.rotate_epoch (), "epoch rotation does not fail while preserving latch");
  check (timeout.submit (frequency_request (timeout, QStringLiteral ("after-rotate"), 14074000)).reason
         == QStringLiteral ("unconfirmed_feedback"),
         "epoch rotation does not clear unconfirmed latch");

  Control recovery {100};
  bind_fixture_epoch (recovery);
  recovery.set_clock_for_test (0);
  recovery.set_observed_state (safe_state (1));
  Control::Dispatch timed_out_business;
  recovery.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      timed_out_business = dispatch;
      Control::Dispatch prepared;
      check (recovery.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "timed-out business operation prepares before execution");
      check (recovery.begin_dispatch (prepared), "timed-out business operation begins once");
    });
  Control::Request start_auto;
  start_auto.request_id = QStringLiteral ("recovery-start");
  start_auto.operation = Control::Operation::StartAutoCall;
  start_auto.server_epoch = recovery.server_epoch ();
  start_auto.state_revision = 1;
  check (recovery.submit (start_auto).status == Control::Status::Pending,
         "business operation can become an uncertain begun request");
  recovery.advance_clock_for_test (100);
  check (recovery.expire () && recovery.result (start_auto.request_id).status == Control::Status::Timeout,
         "begun business operation asserts timeout latch");
  check (!recovery.feedback_business (timed_out_business.request_id, timed_out_business.server_epoch,
                                      1, QStringLiteral ("armed"), true, 2),
         "late business feedback cannot revive the timed-out operation");
  Control::Dispatch recovery_stop;
  recovery.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      recovery_stop = dispatch;
      Control::Dispatch prepared;
      check (recovery.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "stop remains executable while the old operation is uncertain");
      check (recovery.begin_dispatch (prepared), "recovery stop begins through the existing path");
    });
  Control::Request stop_after_timeout;
  stop_after_timeout.request_id = QStringLiteral ("recovery-stop");
  stop_after_timeout.operation = Control::Operation::StopAutoCall;
  stop_after_timeout.server_epoch = recovery.server_epoch ();
  stop_after_timeout.state_revision = 1;
  check (recovery.submit (stop_after_timeout).status == Control::Status::Pending,
         "Stop remains admissible after an uncertain begun operation");
  check (recovery.feedback_business (recovery_stop.request_id, recovery_stop.server_epoch,
                                    1, QStringLiteral ("idle"), false, 2),
         "recovery Stop completes only after idle business readback");
  auto blocked_after_stop = start_auto;
  blocked_after_stop.request_id = QStringLiteral ("recovery-start-again");
  check (recovery.submit (blocked_after_stop).reason == QStringLiteral ("unconfirmed_feedback"),
         "completed recovery Stop does not clear the old-operation latch");

  Control provider {100};
  bind_fixture_epoch (provider);
  provider.set_clock_for_test (0);
  int provider_reads = 0;
  provider.set_observation_provider ([&] {
    ++provider_reads;
    auto state = safe_state (1);
    if (provider_reads > 1) state.safety.ptt = true;
    return state;
  });
  int provider_dispatches = 0;
  provider.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    ++provider_dispatches;
    Control::Dispatch prepared;
    check (!provider.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "unsafe execution-time observation rejects dispatch");
  });
  auto provider_result = provider.submit (frequency_request (provider, QStringLiteral ("reread"), 14074000));
  check (provider_result.status == Control::Status::Rejected
         && provider_result.reason == QStringLiteral ("ptt_active") && provider_dispatches == 1,
          "dispatch re-reads safety and fails closed");

  Control unknown_baseline {100};
  bind_fixture_epoch (unknown_baseline);
  unknown_baseline.set_clock_for_test (0);
  auto unknown_state = safe_state (1);
  unknown_state.frequency_known = false;
  unknown_baseline.set_observed_state (unknown_state);
  unknown_baseline.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    Control::Dispatch prepared;
    check (!unknown_baseline.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "unknown CAT baseline rejects execution");
  });
  auto unknown_result = unknown_baseline.submit (
      frequency_request (unknown_baseline, QStringLiteral ("unknown-baseline"), 14074000));
  check (unknown_result.status == Control::Status::Rejected
         && unknown_result.reason == QStringLiteral ("frequency_baseline_unknown"),
         "frequency dispatch requires a known generation and actual CAT frequency");

  Control provider_reentrant {100};
  bind_fixture_epoch (provider_reentrant);
  provider_reentrant.set_clock_for_test (10);
  provider_reentrant.set_observed_state (safe_state (1));
  bool provider_reentry_checked = false;
  provider_reentrant.set_observation_provider ([&] {
    if (!provider_reentry_checked)
      {
        provider_reentry_checked = true;
        auto nested = provider_reentrant.submit (frequency_request (provider_reentrant,
                                                                      QStringLiteral ("nested"),
                                                                      14074000));
        check (nested.status == Control::Status::Rejected
               && nested.reason == QStringLiteral ("reentrant"),
               "observation provider reentry fails closed");
      }
    return safe_state (1);
  });
  provider_reentrant.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
  auto provider_reentrant_result = provider_reentrant.submit (
      frequency_request (provider_reentrant, QStringLiteral ("outer"), 14074000));
  check (provider_reentrant_result.status == Control::Status::Pending,
         "outer request remains pending after rejected provider reentry");

  Control rotated_by_provider {100};
  bind_fixture_epoch (rotated_by_provider);
  rotated_by_provider.set_clock_for_test (0);
  rotated_by_provider.set_observed_state (safe_state (1));
  int rotate_reads = 0;
  rotated_by_provider.set_observation_provider ([&] {
    ++rotate_reads;
    if (rotate_reads == 1) check (rotated_by_provider.rotate_epoch (), "provider can explicitly rotate idle epoch");
    return safe_state (1);
  });
  int rotated_dispatches = 0;
  rotated_by_provider.set_frequency_dispatcher ([&] (Control::Dispatch const&) { ++rotated_dispatches; });
  auto rotated_request = frequency_request (rotated_by_provider, QStringLiteral ("provider-epoch"), 14074000);
  auto rotated_result = rotated_by_provider.submit (rotated_request);
  check (rotated_result.status == Control::Status::Rejected
         && rotated_result.reason == QStringLiteral ("epoch_changed") && rotated_dispatches == 0,
         "provider epoch rotation invalidates the old request before dispatch");

  Control invalidated_by_provider {100};
  bind_fixture_epoch (invalidated_by_provider);
  invalidated_by_provider.set_clock_for_test (0);
  invalidated_by_provider.set_observed_state (safe_state (1));
  Control::Dispatch invalidated_dispatch;
  invalidated_by_provider.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
      invalidated_dispatch = dispatch;
    });
  auto invalidated_request = frequency_request (invalidated_by_provider,
                                                 QStringLiteral ("provider-stop"), 14074000);
  check (invalidated_by_provider.submit (invalidated_request).status == Control::Status::Pending,
         "provider invalidation fixture enters pending");
  invalidated_by_provider.set_observation_provider ([&] {
      invalidated_by_provider.invalidate_server_epoch (QStringLiteral ("server_stopped"));
      return safe_state (1);
    });
  Control::Dispatch prepared_after_invalidation;
  check (!invalidated_by_provider.prepare_dispatch (invalidated_dispatch.request_id,
                                                     invalidated_dispatch.server_epoch,
                                                     &prepared_after_invalidation),
         "provider invalidation prevents prepare in the same callback");
  check (!invalidated_by_provider.begin_dispatch (invalidated_dispatch),
         "provider invalidation prevents queued begin");
  auto after_provider_stop = invalidated_request;
  after_provider_stop.request_id = QStringLiteral ("provider-stop-new");
  check (invalidated_by_provider.submit (after_provider_stop).reason
         == QStringLiteral ("server_unavailable"),
         "provider invalidation keeps the control unbound until server rebind");

  Control reentrant {100};
  bind_fixture_epoch (reentrant);
  reentrant.set_clock_for_test (0);
  reentrant.set_observed_state (safe_state (1));
  reentrant.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    Control::Dispatch prepared;
    check (reentrant.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
           "reentrant request prepares before feedback");
    check (reentrant.begin_dispatch (prepared), "reentrant request begins before feedback");
    check (reentrant.feedback_frequency (prepared.request_id, prepared.server_epoch,
                                         prepared.expected_generation + 1, prepared.frequency_hz, 2),
           "reentrant handler feedback completes");
    check (reentrant.rotate_epoch (), "completed handler may rotate epoch");
    reentrant.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
    check (reentrant.submit (frequency_request (reentrant, dispatch.request_id, 14075000)).status
           == Control::Status::Pending, "new epoch same id remains pending during old callback");
    throw 1;
  });
  auto reentrant_result = reentrant.submit (frequency_request (reentrant, QStringLiteral ("reentrant"), 14074000));
  check (reentrant_result.status == Control::Status::Rejected, "post-feedback reentry is fail-closed");
  check (reentrant.result (QStringLiteral ("reentrant")).status == Control::Status::Pending,
         "old callback cannot overwrite the new epoch record");

  Control bounded {100};
  bind_fixture_epoch (bounded);
  bounded.set_clock_for_test (0);
  bounded.set_observed_state (safe_state (1));
  bounded.set_frequency_dispatcher ({ });
  for (int index = 0; index < bounded.hard_record_limit (); ++index)
    check (bounded.submit (frequency_request (bounded, QStringLiteral ("r%1").arg (index), 10000000 + index)).status
           == Control::Status::Failed, "failed records remain bounded and retained");
  check (bounded.record_count () == bounded.hard_record_limit (), "hard record limit is retained");
  check (bounded.submit (frequency_request (bounded, QStringLiteral ("overflow"), 16000000)).reason
         == QStringLiteral ("record_limit"), "hard record limit fails closed");
  QString const old_epoch = bounded.server_epoch ();
  check (bounded.rotate_epoch () && bounded.server_epoch () != old_epoch, "explicit epoch rotation clears old records");
  check (bounded.record_count () == 0, "epoch rotation drops prior dedupe window");

  int argc = 1;
  char app_name[] = "jtdx_web_control_test";
  char * argv[] = {app_name, nullptr};
  QCoreApplication app {argc, argv};
  Control event_loop_timeout {20};
  bind_fixture_epoch (event_loop_timeout);
  event_loop_timeout.set_observed_state (safe_state (1));
  event_loop_timeout.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
  check (event_loop_timeout.submit (frequency_request (event_loop_timeout, QStringLiteral ("timer"),
                                                        14074000)).status == Control::Status::Pending,
         "production clock request enters pending");
  QTimer::singleShot (60, &app, &QCoreApplication::quit);
  app.exec ();
  check (event_loop_timeout.result (QStringLiteral ("timer")).status == Control::Status::Timeout,
         "event-loop timer expires production request");

  std::cout << "Web control coordinator checks passed\n";
  return 0;
}
