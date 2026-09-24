#include <QFile>
#include <QByteArray>

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool condition, char const * description)
  {
    if (!condition) {
      std::cerr << "failed: " << description << '\n';
      std::exit (1);
    }
  }
}

int main ()
{
  QFile ui {QStringLiteral (JTDX_SOURCE_DIR "/mainwindow.ui")};
  expect (ui.open (QIODevice::ReadOnly), "open mainwindow UI");
  auto const xml = ui.readAll ();
  expect (!xml.contains ("<height>7</height>"), "button maximum height cannot collapse to seven pixels");
  expect (xml.contains ("name=\"AutoSeqButton\""), "AutoSeq button exists");
  expect (xml.contains ("name=\"AutoTxButton\""), "AutoTx button exists");
  expect (xml.contains ("name=\"stopTxButton\""), "Halt Tx button exists");
  expect (xml.contains ("自动起呼新呼号"), "new callsign menu text exists");
  expect (xml.contains ("自动起呼新呼号波段"), "new callsign band menu text exists");
  expect (xml.contains ("name=\"actionAutoCallNewGridBandMode\""), "new grid band/mode action exists");
  expect (xml.contains ("name=\"actionAutoAnswerDirectedCalls\""), "directed-call action exists");
  expect (xml.contains ("待机时自动应答呼叫本台"), "directed-call action text exists");
  auto const gridAction = xml.indexOf ("<addaction name=\"actionAutoCallNewGrid\"/>");
  auto const gridBandModeAction = xml.indexOf ("<addaction name=\"actionAutoCallNewGridBandMode\"/>");
  auto const callAction = xml.indexOf ("<addaction name=\"actionAutoCallNewCall\"/>");
  expect (gridAction >= 0 && gridAction < gridBandModeAction && gridBandModeAction < callAction,
          "AutoSeq grid actions remain in the required order");

  QFile configuration {QStringLiteral (JTDX_SOURCE_DIR "/Configuration.ui")};
  expect (configuration.open (QIODevice::ReadOnly), "open Configuration UI");
  auto const configurationXml = configuration.readAll ();
  expect (configurationXml.contains ("name=\"autoCallNewGrid_check_box\""), "new grid configuration checkbox exists");
  expect (configurationXml.contains ("name=\"autoCallNewGridBandMode_check_box\""), "new grid band/mode configuration checkbox exists");
  expect (configurationXml.contains ("name=\"autoAnswerDirectedCalls_check_box\""), "directed-call configuration checkbox exists");
  auto const gridCheckbox = configurationXml.indexOf ("name=\"autoCallNewGrid_check_box\"");
  auto const gridBandModeCheckbox = configurationXml.indexOf ("name=\"autoCallNewGridBandMode_check_box\"");
  auto const callCheckbox = configurationXml.indexOf ("name=\"autoCallNewCall_check_box\"");
  expect (gridCheckbox >= 0 && gridCheckbox < gridBandModeCheckbox && gridBandModeCheckbox < callCheckbox,
          "configuration grid checkboxes remain in the required order");
  expect (configurationXml.contains ("日志中从未通联过的四位网格"), "new grid tooltip is whole-grid only");
  expect (configurationXml.contains ("当前波段或模式尚未通联的网格"), "new grid band/mode tooltip is precise");
  expect (configurationXml.contains ("name=\"secondary_udp_info_label\""), "secondary UDP read-only explanation is visible");
  expect (configurationXml.contains ("read-only GridTracker monitoring"), "secondary UDP explanation names GridTracker read-only monitoring");

  QFile configurationSource {QStringLiteral (JTDX_SOURCE_DIR "/Configuration.cpp")};
  expect (configurationSource.open (QIODevice::ReadOnly), "open Configuration source");
  auto const configurationCpp = configurationSource.readAll ();
  expect (configurationCpp.contains ("SeqAutoCallNewGridBandMode\", false"), "new grid band/mode default is false");
  expect (configurationCpp.contains ("autoCallNewGridBandMode_ || m_->autoCallNewCall_"), "rare-target aggregation includes new grid band/mode");
  expect (configurationCpp.contains ("SeqAutoAnswerDirectedCalls\", false"), "directed-call option defaults off");
  expect (configurationCpp.contains ("autoAnswerDirectedCalls_ = ui_->autoAnswerDirectedCalls_check_box->isChecked"), "settings checkbox is persisted");

  QFile mainWindowSource {QStringLiteral (JTDX_SOURCE_DIR "/mainwindow.cpp")};
  expect (mainWindowSource.open (QIODevice::ReadOnly), "open MainWindow source");
  auto const mainWindowCpp = mainWindowSource.readAll ();
  auto const commonActionsStart = mainWindowCpp.indexOf ("void MainWindow::commonActions ()");
  auto const commonActionsEnd = mainWindowCpp.indexOf ("\nvoid ", commonActionsStart + 1);
  expect (commonActionsStart >= 0 && commonActionsEnd > commonActionsStart,
          "commonActions implementation is present for visibility contract check");
  auto const commonActions = mainWindowCpp.mid (commonActionsStart, commonActionsEnd - commonActionsStart);
  expect (!commonActions.contains ("m_wideGraph->show"),
          "mode/reset bookkeeping does not reopen the WideGraph window");
  expect (mainWindowCpp.contains ("WideGraph/visible"),
          "WideGraph visibility has a persisted settings key");
  expect (mainWindowCpp.contains ("m_settings->value (\"WideGraph/visible\", true)"),
          "first-run WideGraph visibility remains compatible and startup can restore it");
  expect (mainWindowCpp.contains ("m_wideGraph->setAttribute (Qt::WA_ShowWithoutActivating)"),
          "startup restoration does not intentionally activate the WideGraph window");
  expect (mainWindowCpp.contains ("m_settings->setValue (\"WideGraph/visible\", true)"),
          "explicit WideGraph menu open remembers visible intent");
  QFile wideGraphSource {QStringLiteral (JTDX_SOURCE_DIR "/widegraph.cpp")};
  expect (wideGraphSource.open (QIODevice::ReadOnly), "open WideGraph source");
  auto const wideGraphCpp = wideGraphSource.readAll ();
  expect (wideGraphCpp.contains ("Q_EMIT closed ()"),
          "user close reports hidden intent without relying on destruction/hide events");
  expect (mainWindowCpp.contains ("m_messageClient->set_mirror (m_secondaryMessageClient)"), "primary owns the telemetry mirror");
  expect (mainWindowCpp.count ("set_mirror (") == 1, "secondary is never assigned a mirror");
  expect (!mainWindowCpp.contains ("m_secondaryMessageClient, &MessageClient::reply"), "secondary has no Reply control connection");
  expect (!mainWindowCpp.contains ("m_secondaryMessageClient, &MessageClient::trigger_CQ"), "secondary has no TriggerCQ control connection");
  expect (mainWindowCpp.contains ("Configuration::udp2_enabled_changed"), "secondary enable changes are wired dynamically");
  expect (mainWindowCpp.contains ("setAutoAnswerDirectedCalls"), "directed-call action updates configuration");
  expect (mainWindowCpp.contains ("actionAutoAnswerDirectedCalls->setChecked(m_config.autoAnswerDirectedCalls())"), "settings and menu stay synchronized");
  expect (!mainWindowCpp.contains ("QUdpSocket"), "secondary no longer sends raw ADIF datagrams");
  expect (mainWindowCpp.contains ("m_autoSeqRecovery.observe_targeted_message"),
          "recovery adapter observes a decoded continuation");
  expect (mainWindowCpp.contains ("decodedtext.isStandardMessage () && mycallinmsg"),
          "recovery continuation requires a standard message addressed to this station");
  expect (mainWindowCpp.contains ("if (!m_manualDecode && !m_diskData)"),
          "manual and disk decodes cannot start a recovery batch");
  expect (mainWindowCpp.contains ("recovery gate withheld manual or disk decode"),
          "pending recovery rejects manual and disk AutoSeq processing");
  expect (mainWindowCpp.contains ("recoveryFreshBatchEligible"),
          "recovery applies the existing TX/PTT guard before any fresh batch fallback");
  expect (mainWindowCpp.contains ("m_autoSeqRecovery.can_process_fresh_batch"),
          "the production recovery gate is centralized in the tested policy helper");
  auto const resumeGuard = mainWindowCpp.indexOf ("if (recoveryFreshBatch && !recoveryFreshBatchEligible)");
  auto const resumeRestore = mainWindowCpp.indexOf ("ui->dxCallEntry->setText (hisCall)", resumeGuard);
  expect (resumeGuard >= 0 && resumeRestore > resumeGuard,
          "a blocked continuation cannot occupy DX before a later fallback batch");
  expect (mainWindowCpp.contains ("completionMessage"),
          "historical signoff handling records the fresh completion token");
  expect (mainWindowCpp.contains ("m_autoSeqRecovery.reconnected_ptt_off"),
          "recovery waits for an actual PTT-off update");
  expect (mainWindowCpp.contains ("clearDX (\" cleared after CAT recovery; waiting for fresh decode\")"),
          "PTT-off release clears the stale DX selection");
  expect (mainWindowCpp.contains ("m_autoSeqRecoveryInternalHalt"),
          "fault halt is distinguished from user halt cancellation");
  auto const recoveryPeriod = mainWindowCpp.indexOf ("applyAutoTxPeriod (hisCall, m_autoSeqRecovery.target_receive_time ())");
  auto const recoveryMessages = mainWindowCpp.indexOf ("genStdMsgs (m_rpt);", recoveryPeriod);
  auto const recoveryEnable = mainWindowCpp.indexOf ("if (!m_enableTx) ui->enableTxButton->click ();", recoveryMessages);
  expect (recoveryPeriod >= 0 && recoveryMessages > recoveryPeriod && recoveryEnable > recoveryMessages,
          "recovery settles period and messages before Enable Tx");
  expect (mainWindowCpp.contains ("recovery rejected unsupported fallback candidate="),
          "unsupported fallback candidate is rejected while recovery ticket remains pending");
  expect (mainWindowCpp.contains ("bool const recoverySelectedNormalCandidate = recoveryFallbackArm"),
          "only an actually armable new candidate may consume recovery ticket");
  expect (mainWindowCpp.contains ("postDecode (true, decodedtext.string (), deCall, grid)"),
          "Web decode metadata reuses the production DecodedText sender call");
  expect (!mainWindowCpp.contains ("postDecode (true, decodedtext.string (), decodedtext.call (), grid)"),
          "Web decode metadata does not mistake the CQ target call for the sender");
  expect (mainWindowCpp.contains ("m_webState->observe_business_state"),
          "Web state reads internal business state from the existing status path");
  expect (mainWindowCpp.contains ("m_curMsgTx.trimmed ().startsWith (QStringLiteral (\"CQ \"))"),
          "CQ state is projected only for an internally selected CQ message");
  expect (mainWindowCpp.contains ("JtdxWebState::project_cq_state"),
          "CQ projection keeps selected, armed and transmitting states distinct");

  QFile hamlibSource {QStringLiteral (JTDX_SOURCE_DIR "/HamlibTransceiver.cpp")};
  expect (hamlibSource.open (QIODevice::ReadOnly), "open Hamlib source");
  auto const hamlibCpp = hamlibSource.readAll ();
  expect (hamlibCpp.contains ("append_hamlib_error_log (message)"),
          "Hamlib ERR callback is persisted to bounded recovery diagnostics");
  expect (hamlibCpp.contains ("overall_consecutive="),
          "poll diagnostics include aggregate failure streak");

  QFile messageClientSource {QStringLiteral (JTDX_SOURCE_DIR "/MessageClient.cpp")};
  expect (messageClientSource.open (QIODevice::ReadOnly), "open MessageClient source");
  auto const messageClientCpp = messageClientSource.readAll ();
  expect (messageClientCpp.contains ("if (m_->mirror_)"), "structured telemetry is mirrored");
  expect (messageClientCpp.contains ("void MessageClient::send_raw_datagram"), "raw datagram API remains available");
  auto const rawDatagram = messageClientCpp.indexOf ("void MessageClient::send_raw_datagram");
  expect (rawDatagram >= 0 && !messageClientCpp.mid (rawDatagram, 800).contains ("mirror_"), "raw datagrams are not mirrored");
  return 0;
}
