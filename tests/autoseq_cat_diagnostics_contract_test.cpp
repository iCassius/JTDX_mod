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

  require (hamlib.contains (QStringLiteral ("throws_to_offline=%14"))
               && hamlib.contains (QStringLiteral (
                    "Decision::soft_ignore != decision ? \"true\" : \"false\"")),
           "hard CAT poll outcomes are marked for the existing offline path");
  require (ordered (polling,
                    QStringLiteral ("poll_exception offline_transition=begin"),
                    QStringLiteral ("offline (message);")),
           "poll exception diagnostic precedes the existing offline transition");
  require (ordered (transceiver,
                    QStringLiteral ("stage=entered failure_signal=not_yet_emitted"),
                    QStringLiteral ("Q_EMIT failure (reason);"))
               && ordered (transceiver,
                           QStringLiteral ("Q_EMIT failure (reason);"),
                           QStringLiteral ("stage=after_emit failure_signal=emitted")),
           "offline entry and completed signal emission are marked at the correct stages");

  auto const configurationFailureStart = configuration.indexOf (
    QStringLiteral ("void Configuration::impl::handle_transceiver_failure"));
  auto const configurationFailure = configuration.mid (configurationFailureStart, 2400);
  require (configurationFailure.contains (QStringLiteral (
               "if (generation != rig_generation_)"))
               && ordered (configurationFailure,
                           QStringLiteral ("if (generation != rig_generation_)"),
                           QStringLiteral ("append_configuration_failure_diagnostic (\"entered\", generation);"))
               && ordered (configurationFailure,
                           QStringLiteral ("if (generation != rig_generation_)"),
                           QStringLiteral ("RigSessionPolicy::failure_action"))
               && ordered (configurationFailure,
                           QStringLiteral ("RigSessionPolicy::failure_action"),
                           QStringLiteral ("close_rig ();"))
               && ordered (configurationFailure,
                           QStringLiteral ("close_rig ();"),
                           QStringLiteral ("append_configuration_failure_diagnostic (\"after_close_rig\", generation);"))
               && configurationFailure.contains (QStringLiteral (
                    "RigSessionPolicy::failure_action"))
               && configurationFailure.contains (QStringLiteral (
                    "FailureAction::forward_to_runtime == action"))
               && configurationFailure.contains (QStringLiteral ("if (forward_to_main_window)"))
               && configurationFailure.contains (QStringLiteral ("Q_EMIT self_->transceiver_failure (reason);"))
               && configurationFailure.contains (QStringLiteral ("message_box_critical (tr (\"Rig failure\"), reason);"))
               && !configurationFailure.contains (QStringLiteral ("isVisible ()")),
           "Configuration rejects stale generations and routes runtime/test failures by session purpose");
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
  require (rigPolicy.contains (QStringLiteral ("failure_action"))
               && rigPolicy.contains (QStringLiteral ("purpose_after_reuse")),
           "Configuration uses the independently tested rig-session policy");
  require (mainwindow.contains (QStringLiteral (
               "connect (&m_config, &Configuration::transceiver_failure, this, &MainWindow::handle_transceiver_failure);")),
           "MainWindow failure connection remains unchanged");

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
               "recovery_online_update online=true ptt_known=true ptt_on=%1"))
               && mainwindow.contains (QStringLiteral ("release_dx_wait_fresh_decode"))
               && mainwindow.contains (QStringLiteral ("retain_ticket_and_dx"))
               && mainwindow.contains (QStringLiteral ("cancel_ticket_context_changed"))
               && mainwindow.contains (QStringLiteral (
                    "decision=%6")),
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
  return 0;
}
