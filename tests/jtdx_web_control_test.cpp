#include "JtdxWebControl.hpp"

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
    state.safety.fresh = true;
    state.safety.business_state_known = true;
    state.state_revision = revision;
    state.frequency_generation = 4;
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
}

int main ()
{
  Control control {100};
  control.set_clock_for_test (1000);
  control.set_observed_state (safe_state (1));

  Control::ObservedState unsafe = safe_state (1);
  unsafe.safety.known = false;
  control.set_observed_state (unsafe);
  auto rejected = control.submit (frequency_request (control, QStringLiteral ("unknown"), 14074000));
  check (rejected.status == Control::Status::Rejected && rejected.reason == QStringLiteral ("safety_unknown_or_stale"),
         "unknown safety fails closed");
  control.set_observed_state (safe_state (1));

  int dispatch_count = 0;
  control.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    ++dispatch_count;
    check (dispatch.server_epoch == control.server_epoch (), "dispatch carries epoch");
    check (control.feedback_frequency (dispatch.request_id, dispatch.server_epoch,
                                       dispatch.expected_generation + 1, dispatch.frequency_hz, 2),
           "synchronous feedback is accepted");
  });
  auto completed = control.submit (frequency_request (control, QStringLiteral ("sync"), 14074000));
  check (completed.status == Control::Status::Completed && completed.reason == QStringLiteral ("feedback_matched"),
         "synchronous dispatch feedback completes");
  check (dispatch_count == 1, "synchronous dispatch occurs once");
  auto duplicate = control.submit (frequency_request (control, QStringLiteral ("sync"), 14074000));
  check (duplicate.status == Control::Status::Completed && dispatch_count == 1,
         "same id and payload is idempotent");
  auto conflict = control.submit (frequency_request (control, QStringLiteral ("sync"), 14075000));
  check (conflict.status == Control::Status::Rejected && conflict.http_status == 409
         && conflict.reason == QStringLiteral ("request_id_conflict") && dispatch_count == 1,
         "same id with different payload is a 409 without dispatch");

  control.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
  auto pending = control.submit (frequency_request (control, QStringLiteral ("pending"), 14075000));
  check (pending.status == Control::Status::Pending && control.has_pending (), "request enters pending");
  check (!control.feedback_frequency (QStringLiteral ("pending"), control.server_epoch (), 4, 14075000, 99),
         "old generation cannot complete despite newer global revision");
  check (!control.feedback_frequency (QStringLiteral ("other"), control.server_epoch (), 5, 14075000, 2),
         "feedback for another request cannot complete pending request");
  check (control.feedback_frequency (QStringLiteral ("pending"), control.server_epoch (), 5, 14075000, 2),
         "matching new generation completes pending request");

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
  });
  auto select_pending = control.submit (select);
  check (select_pending.status == Control::Status::Pending && select_dispatched, "DX enters pending through dispatch");
  check (!control.feedback_select_dx (QStringLiteral ("dx-1"), control.server_epoch (), 8,
                                     QStringLiteral ("JA2LCP"), QStringLiteral ("PM95"), 2),
         "equal DX generation is not completion");
  check (control.feedback_select_dx (QStringLiteral ("dx-1"), control.server_epoch (), 9,
                                    QStringLiteral ("JA2LCP"), QStringLiteral ("PM95"), 2),
         "DX independent feedback generation completes");

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

  Control timeout {100};
  timeout.set_clock_for_test (10);
  timeout.set_observed_state (safe_state (1));
  timeout.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
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

  Control provider {100};
  provider.set_clock_for_test (0);
  int provider_reads = 0;
  provider.set_observation_provider ([&] {
    ++provider_reads;
    auto state = safe_state (1);
    if (provider_reads > 1) state.safety.ptt = true;
    return state;
  });
  int provider_dispatches = 0;
  provider.set_frequency_dispatcher ([&] (Control::Dispatch const&) { ++provider_dispatches; });
  auto provider_result = provider.submit (frequency_request (provider, QStringLiteral ("reread"), 14074000));
  check (provider_result.status == Control::Status::Rejected
         && provider_result.reason == QStringLiteral ("ptt_active") && provider_dispatches == 0,
         "dispatch re-reads safety and fails closed");

  Control rotated_by_provider {100};
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

  Control reentrant {100};
  reentrant.set_clock_for_test (0);
  reentrant.set_observed_state (safe_state (1));
  reentrant.set_frequency_dispatcher ([&] (Control::Dispatch const& dispatch) {
    check (reentrant.feedback_frequency (dispatch.request_id, dispatch.server_epoch,
                                         dispatch.expected_generation + 1, dispatch.frequency_hz, 2),
           "reentrant handler feedback completes");
    check (reentrant.rotate_epoch (), "completed handler may rotate epoch");
    reentrant.set_frequency_dispatcher ([] (Control::Dispatch const&) {});
    throw 1;
  });
  auto reentrant_result = reentrant.submit (frequency_request (reentrant, QStringLiteral ("reentrant"), 14074000));
  check (reentrant_result.status == Control::Status::Rejected, "post-feedback reentry is fail-closed");
  check (reentrant.submit (frequency_request (reentrant, QStringLiteral ("reentrant"), 14075000)).status
         == Control::Status::Pending, "new epoch accepts same id as a fresh record");

  Control bounded {100};
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

  std::cout << "Web control coordinator checks passed\n";
  return 0;
}
