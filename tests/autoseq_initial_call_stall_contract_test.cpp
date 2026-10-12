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

  QString readSource (QString const& relativePath)
  {
    QFile file {QStringLiteral (JTDX_SOURCE_DIR) + QLatin1Char ('/') + relativePath};
    require (file.open (QIODevice::ReadOnly), "source contract fixture opens");
    return QString::fromUtf8 (file.readAll ());
  }

  QString functionBody (QString const& source, QString const& start,
                        QString const& next)
  {
    auto const begin = source.indexOf (start);
    require (begin >= 0, "function start exists");
    auto const end = source.indexOf (next, begin + start.size ());
    require (end > begin, "function terminator exists");
    return source.mid (begin, end - begin);
  }

  bool ordered (QString const& source, QString const& first, QString const& second)
  {
    auto const firstIndex = source.indexOf (first);
    return firstIndex >= 0 && source.indexOf (second, firstIndex + first.size ()) >= 0;
  }
}

int main ()
{
  auto const mainwindow = readSource (QStringLiteral ("mainwindow.cpp"));
  auto const diagnostics = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"),
    QStringLiteral ("void MainWindow::cancelAutoSeqInitialCallStall"));
  auto const evaluation = functionBody (
    mainwindow, QStringLiteral ("bool MainWindow::evaluateAutoSeqInitialCallStallAtBatchEnd"),
    QStringLiteral ("void MainWindow::observeAutoSeqAudioProgress"));
  auto const automaticFlow = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::process_Auto(bool forceCandidate)"),
    QStringLiteral ("void MainWindow::readFromStdout()"));
  auto const readLoop = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::readFromStdout()"),
    QStringLiteral ("void MainWindow::decodeBusy"));
  auto const pttRequest = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::dataSink(qint64 frames)"),
    QStringLiteral ("void MainWindow::startTx2()"));
  auto const startTx = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::startTx2()"),
    QStringLiteral ("void MainWindow::stopTx()"));
  auto const manualDx = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::on_dxCallEntry_textChanged(const QString &t)"),
    QStringLiteral ("void MainWindow::on_dxGridEntry_textChanged(const QString &t)"));
  auto const webDx = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::dispatchWebDx (JtdxWebControl::Dispatch dispatch)"),
    QStringLiteral ("void MainWindow::dispatchWebBusiness"));
  auto const txb1 = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::on_txb1_clicked()"),
    QStringLiteral ("void MainWindow::on_txb2_clicked()"));
  auto const transceiverUpdate = functionBody (
    mainwindow,
    QStringLiteral ("void MainWindow::handle_transceiver_update (Transceiver::TransceiverState const& s)"),
    QStringLiteral ("void MainWindow::appendAutoSeqDiagnostic"));
  auto const haltTx = functionBody (
    mainwindow, QStringLiteral ("void MainWindow::haltTx(QString reason)"),
    QStringLiteral ("void MainWindow::haltTxTuneTimer"));

  require (diagnostics.contains (QStringLiteral ("m_config.is_transceiver_online ()"))
               && !diagnostics.contains (QStringLiteral ("m_config.transceiver_online ()")),
           "diagnostic CAT-state observation cannot call the side-effecting online accessor");
  require (evaluation.contains (QStringLiteral ("m_config.is_transceiver_online ()"))
               && !evaluation.contains (QStringLiteral ("m_config.transceiver_online ()")),
           "stall guard uses the read-only CAT online accessor");
  require (evaluation.contains (QStringLiteral ("m_autoSeqPttReadbackMonotonicMs >="))
               && evaluation.contains (QStringLiteral ("m_autoSeqPttReadbackKnown && !m_autoSeqPttReadbackOn")),
           "release requires same-attempt, known, PTT-off readback");
  require (evaluation.contains (QStringLiteral ("m_skipTx1 && historyStatus == QsoHistory::SREPORT"))
               && evaluation.contains (QStringLiteral ("historyStatus == QsoHistory::RREPORT"))
               && evaluation.contains (QStringLiteral ("m_QSOProgress == REPORT")),
           "skip-Tx1 SREPORT is the only added initial REPORT stage while RREPORT remains protected");
  require (ordered (evaluation, QStringLiteral ("m_qsoHistory.calllist ("),
                           QStringLiteral ("enableTx_mode (false)"))
               && ordered (evaluation, QStringLiteral ("enableTx_mode (false)"),
                           QStringLiteral ("clearDX ("))
               && ordered (evaluation, QStringLiteral ("clearDX ("),
                           QStringLiteral ("return true;")),
           "release records cooldown, disables Enable Tx, clears DX, then returns from this batch");
  require (automaticFlow.contains (QStringLiteral ("autoPeriodCandidate && m_autoseq"))
               && automaticFlow.contains (QStringLiteral ("m_autoSeqInitialCallStall.select"))
               && automaticFlow.contains (QStringLiteral ("AutoCallPolicy::receivedReportOrWeakest (rpt)"))
               && automaticFlow.contains (QStringLiteral ("m_autoSeqStallSuppressedCall == hisCall"))
               && automaticFlow.contains (QStringLiteral ("previousEnableTxReason"))
               && automaticFlow.contains (QStringLiteral ("m_autoSeqStallEnableTxReason = previousEnableTxReason")),
           "automatic ownership uses a sanitized report and respects operator cancellation");
  require (evaluation.contains (QStringLiteral ("m_autoSeqStallEnableTxReason = previousEnableTxReason")),
           "stall cleanup restores the prior temporary Enable Tx reason");
  require (haltTx.contains (QStringLiteral ("halt_tx_requested"))
               && haltTx.contains (QStringLiteral ("), reason, {}, true)")),
           "default structured diagnostics preserve the original haltTx reason and message");
  require (automaticFlow.contains (QStringLiteral ("if (!validDirectedPhase) return;")),
           "mid-batch timeout defers non-continuation AutoSeq candidate processing");
  require (readLoop.contains (QStringLiteral ("m_autoSeqStallTargetedReplySeen = true"))
               && evaluation.contains (QStringLiteral ("historyStatus == QsoHistory::RRREPORT"))
               && ordered (readLoop, QStringLiteral ("evaluateAutoSeqInitialCallStallAtBatchEnd ()"),
                           QStringLiteral ("process_Auto();")),
           "complete-batch check gives a same-batch directed QSO phase priority over release");
  require (ordered (pttRequest,
                     QStringLiteral ("AutoSeqPttObservationPolicy::invalidate_for_request"),
                     QStringLiteral ("g_iptt = 1;"))
                && ordered (pttRequest, QStringLiteral ("g_iptt = 1;"),
                           QStringLiteral ("Q_EMIT m_config.transceiver_ptt (true);"))
                && pttRequest.contains (
                     QStringLiteral ("AutoSeqPttObservationPolicy::invalidate_for_request"))
                && transceiverUpdate.contains (
                     QStringLiteral ("AutoSeqPttObservationPolicy::observe"))
                && transceiverUpdate.contains (
                     QStringLiteral ("pttStateObservation.real_get_advanced ()"))
                && transceiverUpdate.contains (
                     QStringLiteral ("pttStateObservation.snapshot.observed_at_ms"))
                && transceiverUpdate.contains (
                     QStringLiteral ("Action::accepted_legacy_state"))
                && ordered (transceiverUpdate,
                           QStringLiteral ("auto const pttStateObservation"),
                           QStringLiteral ("pttStateObservation.snapshot.observed_at_ms"))
                && ordered (transceiverUpdate,
                           QStringLiteral ("acceptedLegacyObservation"),
                           QStringLiteral ("observe_actual_progress")),
            "new PTT request invalidates prior evidence; strict CAT uses advanced GET serial, while legacy callbacks refresh observation time separately from progress");
  require (pttRequest.contains (QStringLiteral ("m_pttRequestContext = currentPttRequestContext ()"))
               && ordered (transceiverUpdate,
                           QStringLiteral ("cancelPendingPttStartIfContextChanged"),
                           QStringLiteral ("auto pttObservation"))
               && ordered (startTx,
                           QStringLiteral ("cancelPendingPttStartIfContextChanged"),
                           QStringLiteral ("consume_audio")),
           "the request intent snapshot is checked before actual-readback arming and final audio consumption");
  require (manualDx.contains (QStringLiteral ("cancelPendingPttStartIfContextChanged"))
               && webDx.contains (QStringLiteral ("cancelPendingPttStartIfContextChanged"))
               && txb1.contains (QStringLiteral ("cancelPendingPttStartIfContextChanged")),
           "manual/Web DX selection and TX-step changes cancel pending requests without touching consumed TX");

  return 0;
}
