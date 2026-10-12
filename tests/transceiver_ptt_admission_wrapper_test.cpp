#include "EmulateSplitTransceiver.hpp"
#include "TransceiverBase.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <iostream>
#include <memory>
#include <utility>

namespace
{
  class FakeRig final : public TransceiverBase
  {
  public:
    explicit FakeRig (QString const& diagnostics)
      : TransceiverBase {nullptr, diagnostics}
    {}

    int ptt_on_calls {0};
    int ptt_off_calls {0};
    int tx_frequency_calls {0};
    int tx_frequency_about_to_tx_calls {0};
    Frequency rx_frequency {0};
    Frequency tx_frequency {0};
    MODE mode {UNK};
    bool split {false};
    bool ignore_legacy_ptt_write {false};

    void emit_health_fixture ()
    {
      Q_EMIT poll_health_started (41, 100, 500);
      Q_EMIT poll_health_completed (41, 100, 200, 500, false);
      Q_EMIT poll_health_read_result (41, 201, QStringLiteral ("rig_get_ptt"),
                                      0, true, 9, 17);
      Q_EMIT tciframeswritten (23);
      Q_EMIT tci_mod_active (true);
    }

    void emit_strict_off_readback ()
    {
      update_PTT_readback_required (true);
      confirm_PTT_readback (false);
      auto const observed = state ();
      Q_EMIT update (observed, current_control_sequence ());
    }

    bool supports_health_observation () const noexcept override {return true;}

  protected:
    int do_start (JTDXDateTime*) override {return 1;}
    void do_stop () override {}
    void do_frequency (Frequency f, MODE m, bool) override
    {
      rx_frequency = f;
      update_rx_frequency (f);
      if (m != UNK) {mode = m; update_mode (m);}
    }
    void do_tx_frequency (Frequency f, MODE m, bool about_to_tx) override
    {
      ++tx_frequency_calls;
      if (about_to_tx) ++tx_frequency_about_to_tx_calls;
      tx_frequency = f;
      split = f != 0;
      update_other_frequency (f);
      update_split (split);
      if (m != UNK) {mode = m; update_mode (m);}
    }
    void do_mode (MODE m) override {mode = m; update_mode (m);}
    void do_ptt (bool on) override
    {
      if (on) ++ptt_on_calls;
      else ++ptt_off_calls;
      if (on && ignore_legacy_ptt_write) return;
      update_PTT (on);
    }
    ReadOnlyRefreshResult do_read_only_refresh () override
    {
      ReadOnlyRefreshResult result;
      result.status = ReadOnlyRefreshResult::api_success;
      result.operation = QStringLiteral ("fake_read_only_get");
      result.api_return = 0;
      result.protocol_valid = true;
      return result;
    }
  };

  Transceiver::TransceiverState request (bool ptt, Transceiver::Frequency rx,
                                         Transceiver::Frequency tx,
                                         Transceiver::MODE mode)
  {
    Transceiver::TransceiverState state;
    state.online (true);
    state.ptt (ptt);
    state.frequency (rx);
    state.tx_frequency (tx);
    state.split (tx != 0);
    state.mode (mode);
    return state;
  }

  QString source (QString const& relative_path)
  {
    QFile file {QDir {QString::fromUtf8 (JTDX_SOURCE_DIR)}.filePath (relative_path)};
    if (!file.open (QIODevice::ReadOnly)) return {};
    return QString::fromUtf8 (file.readAll ());
  }

  bool run_source_contracts ()
  {
    bool ok {true};
    auto const configuration = source (QStringLiteral ("Configuration.cpp"));
    auto const factory = source (QStringLiteral ("TransceiverFactory.cpp"));
    auto const base = source (QStringLiteral ("TransceiverBase.cpp"));
    auto const split = source (QStringLiteral ("EmulateSplitTransceiver.cpp"));
    auto const mainwindow = source (QStringLiteral ("mainwindow.cpp"));
    auto const dxlab = source (QStringLiteral ("DXLabSuiteCommanderTransceiver.cpp"));
    auto const hrd = source (QStringLiteral ("HRDTransceiver.cpp"));
    auto const omni = source (QStringLiteral ("OmniRigTransceiver.cpp"));
    auto const grantAt = configuration.indexOf (
      QStringLiteral ("void Configuration::impl::transceiver_ptt (bool on)"));
    auto const revokeAt = mainwindow.indexOf (
      QStringLiteral ("void MainWindow::cancelPttStartAndRevokeAdmission"));
    auto const helperEnd = mainwindow.indexOf (
      QStringLiteral ("PttStartPolicy::RequestContext MainWindow::currentPttRequestContext"),
      revokeAt);
    auto const helper = revokeAt >= 0 && helperEnd > revokeAt
      ? mainwindow.mid (revokeAt, helperEnd - revokeAt) : QString {};
    auto const revokeOrder = helper.indexOf (QStringLiteral ("revoke_transceiver_ptt_start"));
    auto const logOrder = helper.indexOf (QStringLiteral ("appendAutoSeqDiagnostic"));
    auto const cancelOrder = helper.indexOf (QStringLiteral ("m_pttStartPolicy.cancel"));
    ok = configuration.contains (QStringLiteral ("ptt_admission_block_->grant ()"))
      && configuration.contains (QStringLiteral ("auto const token = ptt_admission_block_ ? ptt_admission_block_->current () : 0"))
      && grantAt >= 0 && factory.contains (QStringLiteral ("bind_ptt_admission (std::move (ptt_admission))"))
      && factory.indexOf (QStringLiteral ("bind_ptt_admission"))
           < factory.indexOf (QStringLiteral ("move_to_thread_tree"))
      && base.count (QStringLiteral ("ptt_start_admitted (ptt_admission_token)")) >= 3
      && base.contains (QStringLiteral ("do_tx_frequency (s.tx_frequency (), s.mode (), ptt_on);"))
      && split.contains (QStringLiteral ("ptt_start_admitted (ptt_admission_token)"))
      && split.contains (QStringLiteral ("wrapped_->set (emulated_state, sequence_number, ptt_admission_token);"))
      && split.contains (QStringLiteral ("state.ptt_readback_required ()"))
      && dxlab.contains (QStringLiteral ("update_PTT (wrapped_->state ().ptt ());"))
      && hrd.contains (QStringLiteral ("reported_ptt = wrapped_->state ().ptt ();"))
      && hrd.contains (QStringLiteral ("update_PTT (reported_ptt);"))
      && omni.contains (QStringLiteral ("reported_ptt = wrapped_->state ().ptt ();"))
      && omni.contains (QStringLiteral ("update_PTT (reported_ptt);"))
      && revokeOrder >= 0 && logOrder > revokeOrder && cancelOrder > logOrder;

    for (auto const& file : {dxlab, hrd, omni})
      ok = file.contains (
        QStringLiteral ("wrapped_->set (new_state, 0, current_set_admission_token ());")) && ok;

    std::cout << (ok ? "PASS " : "FAIL ")
              << "Configuration grant, create-before-move binding, Base/wrapper gates, nested token forwarding, and revoke-log-cancel ordering source contract\n";
    return ok;
  }
}

int main (int argc, char * argv[])
{
  if (argc < 2)
    {
      std::cerr << "FAIL missing isolated test evidence directory\n";
      return 2;
    }
  auto const diagnostics = QDir::cleanPath (QString::fromLocal8Bit (argv[1]));
  if (!QDir {}.mkpath (diagnostics))
    {
      std::cerr << "FAIL cannot create isolated diagnostic directory\n";
      return 2;
    }
  QCoreApplication app {argc, argv};
  std::cout << "injected_diagnostic_directory="
            << diagnostics.toLocal8Bit ().constData () << '\n';

  bool ok = run_source_contracts ();
  {
    std::unique_ptr<FakeRig> unbound {new FakeRig {diagnostics}};
    auto * raw = unbound.get ();
    raw->start (1, nullptr);
    auto const on = request (true, 14074000, 14076000, Transceiver::USB);
    raw->set (on, 2, 1);
    ok = raw->ptt_on_calls == 0 && !raw->state ().ptt () && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "unbound production base defaults fail-closed\n";
    raw->stop ();
  }

  {
    auto block = std::make_shared<PttAdmissionBlock> ();
    std::unique_ptr<Transceiver> underlying {new FakeRig {diagnostics}};
    auto * raw = static_cast<FakeRig *> (underlying.get ());
    EmulateSplitTransceiver wrapper {std::move (underlying)};
    wrapper.bind_ptt_admission (block);
    auto const old_token = block->grant ();
    block->revoke ();

    Transceiver::TransceiverState latest;
    int updates {0};
    int resolution_signals {0};
    int poll_started {0};
    int poll_completed {0};
    int poll_reads {0};
    int refresh_completed {0};
    int frames_written {0};
    int mod_active {0};
    QObject::connect (&wrapper, &Transceiver::update, &app,
      [&latest, &updates] (Transceiver::TransceiverState const& s, unsigned) {
        latest = s;
        ++updates;
      });
    QObject::connect (&wrapper, &Transceiver::resolution, &app,
      [&resolution_signals] (int) {++resolution_signals;});
    QObject::connect (&wrapper, &Transceiver::poll_health_started, &app,
      [&poll_started] (quint64, qint64, int) {++poll_started;});
    QObject::connect (&wrapper, &Transceiver::poll_health_completed, &app,
      [&poll_completed] (quint64, qint64, qint64, int, bool) {++poll_completed;});
    QObject::connect (&wrapper, &Transceiver::poll_health_read_result, &app,
      [&poll_reads] (quint64, qint64, QString const&, int, bool, unsigned, quint64) {
        ++poll_reads;
      });
    QObject::connect (&wrapper, &Transceiver::read_only_refresh_completed, &app,
      [&refresh_completed] (quint64, qint64, int, QString const&, int, bool, unsigned, quint64) {
        ++refresh_completed;
      });
    QObject::connect (&wrapper, &Transceiver::tciframeswritten, &app,
      [&frames_written] (qint64) {++frames_written;});
    QObject::connect (&wrapper, &Transceiver::tci_mod_active, &app,
      [&mod_active] (bool active) {if (active) ++mod_active;});

    wrapper.start (3, nullptr);
    auto const stale = request (true, 14074000, 14076000, Transceiver::USB);
    wrapper.set (stale, 4, old_token);
    ok = raw->ptt_on_calls == 0 && raw->tx_frequency_about_to_tx_calls == 0
      && raw->rx_frequency == 14074000 && raw->tx_frequency == 0
      && !raw->state ().ptt () && !raw->split && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "revoked EmulateSplit ON cannot select the emulated TX VFO or key wrapped rig\n";

    auto const current_token = block->grant ();
    wrapper.set (stale, 5, current_token);
    ok = raw->ptt_on_calls == 1 && raw->state ().ptt () && ok;
    raw->emit_strict_off_readback ();
    ok = updates > 0 && !latest.ptt () && latest.ptt_readback_required ()
      && latest.ptt_readback_known () && latest.ptt_readback_serial () == 1 && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "required wrapper readback preserves the wrapped actual OFF and its validity serial\n";

    ok = wrapper.supports_health_observation () && ok;
    raw->emit_health_fixture ();
    wrapper.refresh_read_only (77);
    ok = resolution_signals == 1 && poll_started == 1 && poll_completed == 1
      && poll_reads == 1 && refresh_completed == 1
      && frames_written == 1 && mod_active == 1 && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "EmulateSplit forwards resolution, health, refresh, and TCI telemetry\n";
    wrapper.stop ();
  }

  {
    auto block = std::make_shared<PttAdmissionBlock> ();
    std::unique_ptr<Transceiver> underlying {new FakeRig {diagnostics}};
    auto * raw = static_cast<FakeRig *> (underlying.get ());
    raw->ignore_legacy_ptt_write = true;
    EmulateSplitTransceiver wrapper {std::move (underlying)};
    wrapper.bind_ptt_admission (block);
    Transceiver::TransceiverState latest;
    QObject::connect (&wrapper, &Transceiver::update, &app,
      [&latest] (Transceiver::TransceiverState const& s, unsigned) {latest = s;});
    wrapper.start (6, nullptr);
    auto const token = block->grant ();
    auto const on = request (true, 7074000, 7076000, Transceiver::LSB);
    wrapper.set (on, 7, token);
    ok = raw->ptt_on_calls == 1 && !raw->state ().ptt ()
      && latest.ptt () && !latest.ptt_readback_required () && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "legacy EmulateSplit retains FakeIt requested-PTT reporting\n";
    wrapper.stop ();
  }

  return ok ? 0 : 1;
}
