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

  require (hamlib.contains (QStringLiteral ("throws_to_offline=%14"))
               && hamlib.contains (QStringLiteral (
                    "Decision::soft_ignore != decision ? \"true\" : \"false\"")),
           "hard CAT poll outcomes are marked for the existing offline path");
  require (ordered (polling,
                    QStringLiteral ("poll_exception offline_transition=begin"),
                    QStringLiteral ("offline (message);")),
           "poll exception diagnostic precedes the existing offline transition");
  require (ordered (transceiver,
                    QStringLiteral ("transceiver_offline failure_signal=emit"),
                    QStringLiteral ("Q_EMIT failure (reason);")),
           "offline diagnostic precedes the existing failure signal emission");

  require (configuration.contains (QStringLiteral (
               "configuration_failure received=true configuration_visible=%1 forward_to_main_window=%2"))
               && configuration.contains (QStringLiteral ("if (configuration_visible)"))
               && configuration.contains (QStringLiteral ("Q_EMIT self_->transceiver_failure (reason);")),
           "Configuration records visible/forwarding state while preserving its branch");
  require (configuration.contains (QStringLiteral (
               "connect (rig.get (), &Transceiver::failure, this, &Configuration::impl::handle_transceiver_failure);")),
           "Configuration failure connection remains unchanged");
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
               "recovery_online_update online=true ptt_known=true ptt_on=false"))
               && mainwindow.contains (QStringLiteral (
                    "decision=release_dx_wait_fresh_decode"))
               && mainwindow.contains (QStringLiteral (
                    "decision=retain_ticket_and_dx"))
               && mainwindow.contains (QStringLiteral (
                    "decision=cancel_ticket_context_changed")),
           "online recovery logs PTT readback and release/retain/cancel decisions");
  require (ordered (mainwindow,
                    QStringLiteral ("m_autoSeqRecovery.reconnected_ptt_off (recoveredAt);"),
                    QStringLiteral ("clearDX (\" cleared after CAT recovery; waiting for fresh decode\");")),
           "existing PTT-off gate still precedes DX release");
  require (mainwindow.count (QStringLiteral ("m_autoSeqRecovery.cancel ();")) == 1,
           "all recovery ticket cancellations pass through the diagnostic wrapper");
  require (mainwindow.contains (QStringLiteral ("reason_present=true"))
               && !mainwindow.contains (QStringLiteral ("failure=%1; online=%2")),
           "failure diagnostics record reason presence without logging CAT error text");
  return 0;
}
