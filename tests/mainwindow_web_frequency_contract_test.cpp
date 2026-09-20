#include "JtdxWebControl.hpp"

#include <QFile>
#include <QJsonObject>

#include <cstdlib>
#include <iostream>

namespace
{
  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  JtdxWebControl::ObservedState safe_mainwindow_observation ()
  {
    JtdxWebControl::ObservedState state;
    state.safety.known = true;
    state.safety.fresh = true;
    state.safety.rig_online = true;
    state.safety.monitoring = true;
    state.safety.business_state_known = true;
    state.state_revision = 10;
    state.frequency_generation = 4;
    state.frequency_known = true;
    state.actual_frequency_hz = 14074000;
    return state;
  }
}

int main ()
{
  QFile main_window {QStringLiteral (JTDX_SOURCE_DIR "/mainwindow.cpp")};
  check (main_window.open (QIODevice::ReadOnly), "open MainWindow source");
  QByteArray const source = main_window.readAll ();
  check (source.contains ("configuration.enable_frequency_control = m_config.web_ui_frequency_control_enabled ()"),
         "MainWindow forwards the explicit frequency-control setting");
  check (source.contains ("configuration.enable_dx_control = m_config.web_ui_dx_control_enabled ()"),
         "MainWindow forwards the explicit DX-control setting");
  check (source.contains ("result.safety.rig_online = m_rigOk && m_config.is_transceiver_online ()"),
         "MainWindow adapter requires both CAT observation and configuration online state");
  check (source.contains ("result.safety.monitoring = m_monitoring")
             && source.contains ("result.safety.start2 = m_start2")
             && source.contains ("result.safety.tune = m_tune")
             && source.contains ("result.safety.auto_tx = m_autoTx")
             && source.contains ("result.safety.iptt = g_iptt != 0"),
         "MainWindow adapter projects the live TX safety fields");
  auto const prepare = source.indexOf ("m_webControl->prepare_dispatch");
  auto const begin = source.indexOf ("m_webControl->begin_dispatch", prepare);
  auto const band_change = source.indexOf ("band_changed (validation.frequency_hz)", begin);
  check (prepare >= 0 && begin > prepare && band_change > begin,
         "MainWindow dispatch prepares, begins, then reuses band_changed");
  auto const dispatch = source.indexOf ("void MainWindow::dispatchWebFrequency");
  auto const next_function = source.indexOf ("void MainWindow::on_actionOpenWebUi_triggered", dispatch);
  QByteArray const dispatch_body = source.mid (dispatch, next_function - dispatch);
  check (!dispatch_body.contains ("on_enableTxButton_clicked")
             && !dispatch_body.contains ("enableTx_mode")
             && !dispatch_body.contains ("haltTx"),
         "frequency adapter does not call TX/PTT entry points");
  check (source.contains ("m_webState->observe_rig (s.online (), s.frequency (), s.tx_frequency (), s.ptt ())")
             && source.contains ("m_webControl->feedback_frequency"),
         "MainWindow completes frequency only from rig observation feedback");
  check (source.contains ("m_webState->observe_web_dx_selection")
             && source.contains ("m_webControl->feedback_select_dx"),
         "MainWindow completes DX only from the independent selection observation");
  auto const dx_dispatch = source.indexOf ("void MainWindow::dispatchWebDx");
  auto const dx_next_function = source.indexOf ("void MainWindow::on_actionOpenWebUi_triggered", dx_dispatch);
  QByteArray const dx_dispatch_body = source.mid (dx_dispatch, dx_next_function - dx_dispatch);
  check (dx_dispatch >= 0 && dx_next_function > dx_dispatch
             && !dx_dispatch_body.contains ("processMessage")
             && !dx_dispatch_body.contains ("clearDX")
             && !dx_dispatch_body.contains ("genStdMsgs")
             && !dx_dispatch_body.contains ("enableTx_mode")
             && !dx_dispatch_body.contains ("haltTx"),
         "DX adapter only updates the input projection and cannot enter QSO or TX paths");

  JtdxWebControl control {100};
  control.set_clock_for_test (0);
  control.bind_server_epoch (control.server_epoch ());
  auto unsafe = safe_mainwindow_observation ();
  unsafe.safety.rig_online = false;
  control.set_observed_state (unsafe);
  control.set_frequency_dispatcher ([] (JtdxWebControl::Dispatch const&) {});
  JtdxWebControl::Request request;
  request.request_id = QStringLiteral ("rig-none");
  request.server_epoch = control.server_epoch ();
  request.state_revision = unsafe.state_revision;
  request.frequency_hz = 14075000;
  auto rejected = control.submit (request);
  check (rejected.status == JtdxWebControl::Status::Rejected
             && rejected.reason == QStringLiteral ("rig_offline"),
         "Rig=None isolation rejects frequency dispatch before any adapter callback");

  control.set_observed_state (safe_mainwindow_observation ());
  JtdxWebControl::Dispatch captured;
  control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      captured = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "isolated MainWindow adapter prepares with a fresh observation");
      check (control.begin_dispatch (prepared),
             "isolated MainWindow adapter begins exactly once");
    });
  request.request_id = QStringLiteral ("safe-frequency");
  auto pending = control.submit (request);
  check (pending.status == JtdxWebControl::Status::Pending && captured.frequency_hz == 14075000,
         "safe isolated adapter reaches the pending state without hardware I/O");
  check (control.feedback_frequency (captured.request_id, captured.server_epoch,
                                     captured.expected_generation + 1, captured.frequency_hz, 11),
         "isolated adapter requires a matching later frequency readback");
  check (control.result (request.request_id).status == JtdxWebControl::Status::Completed,
         "matching isolated readback completes the request");
  return 0;
}
