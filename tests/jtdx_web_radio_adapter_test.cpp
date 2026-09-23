#include "JtdxWebControl.hpp"
#include "JtdxWebRadioAdapter.hpp"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

namespace
{
  using Control = JtdxWebControl;

  void check (bool condition, char const * message)
  {
    if (!condition)
      {
        std::cerr << "failed: " << message << '\n';
        std::exit (1);
      }
  }

  Control::ObservedState state (quint64 revision = 1)
  {
    Control::ObservedState value;
    value.state_revision = revision;
    value.radio_state_known = true;
    value.safety.known = true;
    value.safety.rig_online = true;
    value.safety.monitoring = true;
    value.safety.business_state_known = true;
    return value;
  }

  Control::Request request (Control const& control, QString id, QString action, bool value = false)
  {
    Control::Request result;
    result.request_id = std::move (id);
    result.operation = Control::Operation::Radio;
    result.server_epoch = control.server_epoch ();
    result.radio_action = std::move (action);
    result.radio_value = value;
    return result;
  }
}

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  Control control {100};
  control.bind_server_epoch (control.server_epoch ());
  control.set_clock_for_test (1000);
  Control::ObservedState observed = state (1);
  control.set_observation_provider ([&] { return observed; });
  int native_calls = 0;
  control.set_business_dispatcher ([&] (Control::Dispatch const& incoming) {
      JtdxWebRadioAdapter::dispatch (control, incoming,
        [&] (Control::Dispatch const& action, QString *) {
          ++native_calls;
          if (action.radio_action == QStringLiteral ("enable-tx")) observed.safety.tx_enabled = action.radio_value;
          else if (action.radio_action == QStringLiteral ("stop-tx"))
            {
              observed.safety.tx_enabled = false;
              observed.safety.transmitting = false;
              observed.safety.ptt = false;
              observed.safety.tune = false;
            }
          else if (action.radio_action == QStringLiteral ("sync")) observed.radio_sync = action.radio_value;
          else if (action.radio_action == QStringLiteral ("multi-decode"))
            observed.radio_multi_decode = action.radio_value;
          else if (action.radio_action == QStringLiteral ("log-qso"))
            { observed.radio_log_dialog_open = true; observed.radio_qso_draft = {{"call", "K1ABC"}}; }
          else if (action.radio_action == QStringLiteral ("log-qso-cancel"))
            { observed.radio_log_dialog_open = false; observed.radio_qso_draft = {}; }
          else if (action.radio_action == QStringLiteral ("log-qso-confirm"))
            { observed.radio_log_dialog_open = false; observed.radio_qso_draft = {}; ++observed.radio_qso_generation; }
          else if (action.radio_action != QStringLiteral ("clear-windows")) return false;
          return true;
        },
        [&] {
          ++observed.state_revision;
          return observed;
        });
    });

  auto submit = [&] (QString id, QString action, bool value = false) {
    return control.submit (request (control, std::move (id), std::move (action), value));
  };
  check (submit ("sync-1", "sync", true).status == Control::Status::Completed,
         "sync uses one prepare/begin and completes from a newer readback");
  check (submit ("multi-1", "multi-decode", true).status == Control::Status::Completed,
         "multi-decode uses its intended state path");
  check (submit ("clear-1", "clear-windows").status == Control::Status::Completed,
         "clear-windows completes after command return and fresh known state");
  check (submit ("qso-open", "log-qso").status == Control::Status::Completed,
         "QSO action opens an editable draft, not a persisted record");
  check (submit ("qso-cancel", "log-qso-cancel").status == Control::Status::Completed,
         "QSO draft cancel remains a separate explicit action");
  check (submit ("qso-open-2", "log-qso").status == Control::Status::Completed,
         "QSO draft can be reopened");
  Control::Request confirm = request (control, "qso-save", "log-qso-confirm");
  confirm.radio_qso = {{"call", "K1ABC"}};
  check (control.submit (confirm).status == Control::Status::Completed,
         "QSO confirmation waits for a successful generation change and closed draft");
  check (native_calls == 7, "each successful user request invokes its action exactly once");
  check (submit ("sync-1", "sync", true).status == Control::Status::Completed
             && native_calls == 7,
         "same request id replay returns the stored result without reapplying the action");

  // Reproduce the old MainWindow route: its outer prepare/begin consumed the
  // request, so the radio adapter's own prepare must refuse to call the slot.
  // Use a fresh controller to assert the pre-fix double-registration defect.
  Control old_control {100};
  old_control.bind_server_epoch (old_control.server_epoch ());
  old_control.set_clock_for_test (2000);
  auto old_state = state (1);
  old_control.set_observed_state (old_state);
  int old_calls = 0;
  old_control.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      Control::Dispatch outer;
      if (!old_control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &outer)
          || !old_control.begin_dispatch (outer)) return;
      JtdxWebRadioAdapter::dispatch (old_control, dispatch,
        [&] (Control::Dispatch const&, QString *) { ++old_calls; return true; },
        [&] { ++old_state.state_revision; return old_state; });
    });
  auto old_result = old_control.submit (request (old_control, "double-begin", "sync", true));
  check (old_result.status == Control::Status::Pending && old_calls == 0,
         "regression fixture proves the former outer+adapter double begin skipped native action");
  old_control.advance_clock_for_test (101);
  check (old_control.expire () && old_control.result ("double-begin").status == Control::Status::Timeout,
         "unapplied old-route request expires instead of claiming completion");

  Control local_timeout {100};
  local_timeout.bind_server_epoch (local_timeout.server_epoch ());
  local_timeout.set_clock_for_test (3000);
  auto local_state = state (1);
  local_timeout.set_observed_state (local_state);
  local_timeout.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      Control::Dispatch prepared;
      if (local_timeout.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared))
        local_timeout.begin_dispatch (prepared); // Deliberately no feedback.
    });
  check (local_timeout.submit (request (local_timeout, "local-timeout", "sync", true)).status
             == Control::Status::Pending,
         "local operation can remain pending awaiting its bounded timeout");
  local_timeout.advance_clock_for_test (101);
  check (local_timeout.expire ()
             && local_timeout.submit (request (local_timeout, "after-local-timeout", "sync", false)).status
                    == Control::Status::Pending,
         "ordinary local UI timeout does not block a different local control");

  Control hazardous {100};
  hazardous.bind_server_epoch (hazardous.server_epoch ());
  hazardous.set_clock_for_test (4000);
  auto hazardous_state = state (1);
  hazardous.set_observed_state (hazardous_state);
  int stop_calls = 0;
  hazardous.set_business_dispatcher ([&] (Control::Dispatch const& dispatch) {
      Control::Dispatch prepared;
      if (!hazardous.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared)
          || !hazardous.begin_dispatch (prepared)) return;
      if (prepared.radio_action == QStringLiteral ("stop-tx"))
        {
          ++stop_calls;
          hazardous_state.safety.tx_enabled = false;
          hazardous_state.safety.transmitting = false;
          hazardous_state.safety.ptt = false;
          hazardous_state.safety.tune = false;
          ++hazardous_state.state_revision;
          hazardous.feedback_radio (prepared.request_id, prepared.server_epoch,
                                    hazardous_state.state_revision, hazardous_state);
        }
    });
  check (hazardous.submit (request (hazardous, "arm-unknown", "enable-tx", true)).status
             == Control::Status::Pending,
         "hazardous start remains pending absent safety readback");
  hazardous.advance_clock_for_test (101);
  check (hazardous.expire ()
             && hazardous.submit (request (hazardous, "blocked-sync", "sync", true)).reason
                    == QStringLiteral ("unconfirmed_feedback"),
         "unknown TX-enable outcome retains the safety lock against unrelated operations");
  check (hazardous.submit (request (hazardous, "stop-recovery", "stop-tx")).status
             == Control::Status::Completed && stop_calls == 1,
         "Stop remains admissible and is not replayed automatically");
  return 0;
}
