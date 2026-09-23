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
  QFile display_text {QStringLiteral (JTDX_SOURCE_DIR "/displaytext.cpp")};
  check (display_text.open (QIODevice::ReadOnly), "open desktop decode geography projection");
  QByteArray const display_text_source = display_text.readAll ();
  QFile main_window_ui {QStringLiteral (JTDX_SOURCE_DIR "/mainwindow.ui")};
  check (main_window_ui.open (QIODevice::ReadOnly), "open MainWindow UI definition");
  QByteArray const main_window_ui_source = main_window_ui.readAll ();
  QFile web_app {QStringLiteral (JTDX_SOURCE_DIR "/resources/web-ui/app.js")};
  check (web_app.open (QIODevice::ReadOnly), "open Web UI source");
  QByteArray const web_app_source = web_app.readAll ();
  QFile web_index {QStringLiteral (JTDX_SOURCE_DIR "/resources/web-ui/index.html")};
  check (web_index.open (QIODevice::ReadOnly), "open Web UI document");
  QByteArray const web_index_source = web_index.readAll ();
  QFile web_style {QStringLiteral (JTDX_SOURCE_DIR "/resources/web-ui/style.css")};
  check (web_style.open (QIODevice::ReadOnly), "open Web UI stylesheet");
  QByteArray const web_style_source = web_style.readAll ();
  check (!web_index_source.contains ("role=\"dialog\"")
             && !web_index_source.contains ("aria-modal=\"true\"")
             && !web_app_source.contains ("requestConfirmation")
             && !web_app_source.contains ("confirm_dialog")
             && web_app_source.contains ("function startConnection()")
             && !web_app_source.contains ("Authorization")
             && !web_app_source.contains ("token_input")
             && !web_index_source.contains ("token_input")
             && !web_index_source.contains ("freshness")
             && !web_index_source.contains ("operations_card")
             && web_index_source.contains ("目标频率（Hz）")
             && web_index_source.contains ("id=\"dx_call\"")
             && web_index_source.contains ("id=\"dx_grid\"")
             && !web_index_source.contains ("dx_call_input")
             && !web_index_source.contains ("dx_grid_input")
             && !web_index_source.contains ("dx_apply")
             && web_app_source.contains ("decode.message || \"\"")
             && web_app_source.contains ("decode.entity || decode.country || \"\"")
             && web_app_source.contains ("decode.province || \"\"")
             && web_app_source.contains ("sendSelectDx(decode)")
             && web_app_source.contains ("if (snapshot.online !== true) return \"主程序未在线\";")
             && web_app_source.contains ("if (action === \"stop-tx\") return \"\";")
             && web_app_source.contains ("radioUnknown = radioRequest.preserveUnknown === true;")
             && web_app_source.contains ("radioAbort) radioAbort.abort();")
             && web_style_source.contains ("grid-template-rows: auto auto")
             && web_style_source.contains ("overflow-y: auto; overflow-x: hidden")
             && web_style_source.contains (".decode-card { min-width: 0; }")
             && web_style_source.contains (".radio-actions { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr));")
             && web_index_source.contains ("启用发射") && web_index_source.contains ("终止发射")
             && web_index_source.contains ("记录通联") && web_index_source.contains ("清空窗口")
             && web_index_source.contains ("多次解码")
             && !web_index_source.contains ("id=\"radio_agc\"")
             && !web_index_source.contains ("id=\"radio_cq\"")
             && !web_index_source.contains ("web_tx_index")
             && !web_style_source.contains (".decode button { display: none")
             && !web_index_source.contains ("MHz")
             && !web_app_source.contains ("Date.now")
             && web_app_source.contains ("/api/v1/state")
             && web_app_source.contains ("snapshot.jtdx_time_ms")
             && web_app_source.contains ("reconcileFrequency(snapshot);")
             && web_app_source.contains ("reconcileDx(snapshot);")
             && web_app_source.contains ("reconcileBusiness(snapshot);")
             && web_app_source.contains ("reconcileRadio(snapshot);")
             && web_app_source.contains ("nextCycleMode !== cycleMode")
             && web_app_source.contains ("window.addEventListener(\"pageshow\", () => calibrateCycle(\"pageshow\"))")
             && web_style_source.contains (".cycle-strip { position: sticky")
             && web_style_source.contains ("main { max-width: 1100px; margin: auto; padding: 0 18px 42px; }")
             && web_index_source.indexOf ("id=\"cycle_strip\"")
                  < web_index_source.indexOf ("class=\"hero\"")
             && web_index_source.indexOf ("class=\"hero\"")
                  < web_index_source.indexOf ("class=\"workbench\"")
             && web_index_source.indexOf ("class=\"workbench-left\"")
                  < web_index_source.indexOf ("class=\"workbench-right\"")
             && web_index_source.indexOf ("class=\"card radio-state-card\"")
                  > web_index_source.indexOf ("CQ / AutoSeq")
             && !web_index_source.contains ("运行与接收")
             && !web_index_source.contains ("actual-frequency")
             && !web_index_source.contains ("frequency-control-help")
             && !web_index_source.contains ("frequency_candidates_meta")
             && !web_index_source.contains ("frequency_control_status")
             && !web_index_source.contains ("dx_selection_status")
             && !web_index_source.contains ("business_control_status")
             && !web_index_source.contains ("radio_control_status")
             && !web_index_source.contains ("radio_qso_status")
             && !web_index_source.contains ("等待候选")
             && !web_index_source.contains ("等待状态")
             && !web_index_source.contains ("尚未应用")
             && !web_index_source.contains ("尚未发送")
             && !web_index_source.contains ("页面显示和本地控制结果不替代")
             && !web_index_source.contains ("确认前仅保存在当前 Web 会话")
             && !web_index_source.contains ("提交前不会写入 ADIF")
             && !web_index_source.contains ("选择并编辑当前 Tx")
             && !web_style_source.contains (".frequency-control-status")
             && !web_style_source.contains (".operations-card")
             && !web_style_source.contains (".confirm-dialog")
             && web_app_source.contains ("strip.dataset.calibrationRttMinMs")
             && web_app_source.contains ("strip.dataset.calibrationRttMaxMs")
             && web_app_source.contains ("strip.dataset.midpointUncertaintyMs")
             && web_app_source.contains ("strip.dataset.calibrationHistory")
             && web_app_source.contains ("estimatedJtdx % cyclePeriodMs")
             && web_app_source.contains ("calibrateCycle(\"reconnect\")")
             && web_app_source.contains ("calibrateCycle(\"visibility\")")
             && web_app_source.contains ("const sequence = ++cycleCalibrationSequence")
             && web_app_source.contains ("input.dataset.dirty !== \"1\"")
             && web_app_source.contains ("input.value = String(currentSnapshot.frequency)")
             && !web_app_source.contains ("frequency_candidates_meta")
             && !web_app_source.contains ("frequency_control_status")
             && web_index_source.contains ("class=\"workbench\"")
             && web_index_source.contains ("class=\"workbench-left\"")
             && web_index_source.contains ("class=\"workbench-right\"")
             && !web_index_source.contains ("epoch")
             && !web_index_source.contains ("revision")
             && !web_index_source.contains ("开发")
             && web_style_source.contains ("@media (max-width: 850px)")
             && web_style_source.contains (".workbench-right { grid-template-columns: minmax(0, 1fr); }")
             && web_style_source.contains ("overflow-x: hidden")
             && !web_app_source.contains ("globalThis.confirm"),
         "UI omits Web-specific confirmation/auth/freshness cards and uses the JTDX clock in a responsive Hz layout");
  check (!source.contains ("enable_frequency_control") && !source.contains ("enable_dx_control")
             && !source.contains ("enable_automation_control") && !source.contains ("enable_radio_control"),
         "MainWindow does not configure per-feature Web capability gates");
  auto const lang_menu = main_window_ui_source.indexOf ("<addaction name=\"menuLang\"/>");
  auto const web_menu = main_window_ui_source.indexOf ("<addaction name=\"menuWebUI\"/>");
  auto const help_menu = main_window_ui_source.indexOf ("<addaction name=\"menuHelp\"/>");
  check (lang_menu >= 0 && web_menu > lang_menu && help_menu > web_menu,
         "Language-WebUI-Help menus stay in the required top-level order");
  check (main_window_ui_source.contains ("<action name=\"actionWebUiEnabled\">")
             && source.contains ("ui->actionOpenWebUi->setEnabled (true)")
             && source.contains ("ui->actionOpenWebUi->setEnabled (false)")
             && source.contains ("m_config.set_web_ui_status (QStringLiteral (\"错误\")"),
         "Web UI menu enable/open state has explicit success and failure branches");
  check (source.contains ("bool MainWindow::openWebLogQsoDraft ()")
             && source.contains ("bool MainWindow::cancelWebLogQsoDraft ()")
             && source.contains ("bool MainWindow::commitWebLogQsoDraft")
             && source.contains ("QStringLiteral (\"duplicate_qso\")")
             && source.contains ("m_logDlg->acceptWebQSO (&write_reason)")
             && source.contains ("m_logDlg->done (QDialog::Accepted)"),
         "Web QSO uses an editable draft, explicit commit, and duplicate guard");
  check (source.contains ("result.safety.rig_online = m_rigOk && m_config.is_transceiver_online ()"),
         "MainWindow adapter requires both CAT observation and configuration online state");
  check (!source.contains ("current.safety.fresh")
             && !source.contains ("result.safety.fresh"),
         "MainWindow Web readback no longer depends on a removed freshness snapshot");
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
  auto const next_function = source.indexOf ("void MainWindow::dispatchWebDx", dispatch);
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
  check (source.contains ("QString entity = entity_data.value (2).trimmed ()")
             && source.contains ("QString continent = entity_data.value (0).trimmed ()")
             && source.contains ("CallsignLocation::chinaProvince (callsign, entity_data.value (1).trimmed ())")
             && display_text_source.contains ("cntry = items[2]")
             && display_text_source.contains ("CallsignLocation::chinaProvince(checkCall, mpx)"),
         "Web geography matches desktop entity naming and uses only the existing China province helper");
  auto const dx_dispatch = source.indexOf ("void MainWindow::dispatchWebDx");
  auto const dx_next_function = source.indexOf ("void MainWindow::applyWebStartCq", dx_dispatch);
  QByteArray const dx_dispatch_body = source.mid (dx_dispatch, dx_next_function - dx_dispatch);
  check (dx_dispatch >= 0 && dx_next_function > dx_dispatch
             && !dx_dispatch_body.contains ("processMessage")
             && !dx_dispatch_body.contains ("clearDX")
             && !dx_dispatch_body.contains ("genStdMsgs")
             && !dx_dispatch_body.contains ("enableTx_mode")
             && !dx_dispatch_body.contains ("haltTx"),
         "DX adapter only updates the input projection and cannot enter QSO or TX paths");
  check (source.contains ("void MainWindow::dispatchWebBusiness")
             && source.contains ("m_webControl->feedback_business"),
         "MainWindow forwards CQ/AutoSeq commands through a separate business-state readback adapter");
  auto const radio_route = source.indexOf ("void MainWindow::dispatchWebBusiness");
  auto const radio_adapter = source.indexOf ("void MainWindow::dispatchWebRadio", radio_route);
  auto const radio_route_body = source.mid (radio_route, radio_adapter - radio_route);
  check (radio_route >= 0 && radio_adapter > radio_route
             && radio_route_body.indexOf ("Operation::Radio") >= 0
             && radio_route_body.indexOf ("dispatchWebRadio")
                    < radio_route_body.indexOf ("m_webControl->prepare_dispatch")
             && source.contains ("JtdxWebRadioAdapter::dispatch (*m_webControl, dispatch"),
         "Radio dispatch reaches its single prepare/begin adapter before the business path");
  auto const radio_end = source.indexOf ("void MainWindow::on_actionOpenWebUi_triggered", radio_adapter);
  auto const radio_body = source.mid (radio_adapter, radio_end - radio_adapter);
  check (radio_body.contains ("enableTx_mode (action.radio_value)")
             && radio_body.contains ("on_stopTxButton_clicked ()")
             && radio_body.contains ("on_EraseButton_clicked ()")
             && radio_body.contains ("ui->syncButton->setChecked (action.radio_value)")
             && radio_body.contains ("on_swlButton_clicked (action.radio_value)")
             && radio_body.contains ("commitWebLogQsoDraft (action.radio_qso"),
         "the six Web radio controls reuse the native desktop entry points");
  check (web_app_source.contains ("readback.tx_enabled === request.value")
             && web_app_source.contains ("applyRadioReadback(payload, request)")
             && web_app_source.contains ("currentSnapshot?.tx_enabled === false")
             && web_app_source.contains ("snapshot?.tx_enabled === true")
             && web_app_source.contains ("radio_enable_tx: [\"enable-tx\", false]"),
         "Enable Tx mirrors the native checkable button and confirms either requested boolean state");
  check (radio_body.contains ("m_webRadioPendingRequestId = dispatch.request_id")
             && radio_body.contains ("JtdxWebRadioAdapter::observe")
             && radio_body.contains ("tryCompletePendingWebRadio ()"),
         "a nonterminal radio action is later reconciled by state publication without replaying dispatch");
  auto const rig_update = source.indexOf ("void MainWindow::handle_transceiver_update");
  auto const rig_failure = source.indexOf ("void MainWindow::handle_transceiver_failure", rig_update);
  auto const rig_update_body = source.mid (rig_update, rig_failure - rig_update);
  check (rig_update >= 0 && rig_failure > rig_update
             && rig_update_body.indexOf ("m_webState->observe_rig")
                    < rig_update_body.indexOf ("tryCompletePendingWebRadio ()")
             && source.contains ("const_cast<MainWindow *> (this)->tryCompletePendingWebRadio ();"),
         "subsequent rig and status observations can complete a pending radio request");
  auto const native_stop = source.indexOf ("void MainWindow::on_stopTxButton_clicked");
  auto const native_stop_end = source.indexOf ("void MainWindow::rigOpen", native_stop);
  auto const native_stop_body = source.mid (native_stop, native_stop_end - native_stop);
  check (native_stop >= 0 && native_stop_end > native_stop
             && native_stop_body.contains ("if (m_tune) stop_tuning ();" )
             && native_stop_body.contains ("if (m_enableTx and !m_tuneup) enableTx_mode (false);"),
         "Stop preserves native tune cancellation and the tuneup-specific Enable Tx rule");
  auto const cq_apply = source.indexOf ("void MainWindow::applyWebStartCq");
  auto const cq_next = source.indexOf ("void MainWindow::applyWebStartAutoCall", cq_apply);
  auto const cq_body = source.mid (cq_apply, cq_next - cq_apply);
  check (cq_apply >= 0 && cq_next > cq_apply
             && cq_body.contains ("on_txb6_clicked ()")
             && cq_body.contains ("enableTx_mode (true)"),
         "Web CQ reuses the existing CQ entry and then arms Enable Tx through its safety entry");
  auto const stop_apply = source.indexOf ("void MainWindow::applyWebStopAutoCall");
  auto const business_dispatch = source.indexOf ("void MainWindow::dispatchWebBusiness", stop_apply);
  auto const stop_body = source.mid (stop_apply, business_dispatch - stop_apply);
  check (stop_apply >= 0 && business_dispatch > stop_apply
             && stop_body.indexOf ("on_stopTxButton_clicked ()")
                    < stop_body.indexOf ("on_AutoSeqButton_clicked (false)"),
         "Web stop halts TX first and disables AutoSeq so later decodes cannot resume scheduling");
  auto const auto_start = source.indexOf ("void MainWindow::applyWebStartAutoCall");
  check (source.contains ("AutoSeq is decode-driven in JTDX")
             && auto_start >= 0
             && !source.mid (auto_start, stop_apply - auto_start).contains ("process_Auto ("),
         "Web auto-call is explicitly an AutoSeq arm, not an invented immediate decode call");

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
