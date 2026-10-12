#include "TransceiverBase.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QDir>

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{
  enum class ReadBehavior {success, api_failure, throws, unsupported};

  struct SharedState
  {
    std::mutex mutex;
    std::condition_variable changed;
    int start_calls {0};
    int stop_calls {0};
    int frequency_writes {0};
    int tx_frequency_writes {0};
    int mode_writes {0};
    int ptt_writes {0};
    int get_calls {0};
    int update_calls {0};
    int refresh_events {0};
    int refresh_status {-1};
    unsigned refresh_sequence {0};
    quint64 refresh_serial {0};
    quint64 update_serial {0};
    bool update_online {false};
    bool update_ptt_known {false};
    bool update_ptt {false};
    bool destroyed {false};
    bool suppress_updates {false};
  };

  class FakeRig final : public TransceiverBase
  {
  public:
    FakeRig (std::shared_ptr<SharedState> shared, ReadBehavior behavior,
             QString const& diagnostics)
      : TransceiverBase {nullptr, diagnostics}
      , shared_ {std::move (shared)}
      , behavior_ {behavior}
    {}

    bool supports_health_observation () const noexcept override
    {
      return ReadBehavior::unsupported != behavior_;
    }

  protected:
    int do_start (JTDXDateTime*) override
    {
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->start_calls;
        shared_->changed.notify_all ();
      }
      update_PTT_readback_required (true);
      confirm_PTT_readback (false);
      return 1;
    }

    void do_stop () override
    {
      std::lock_guard<std::mutex> lock {shared_->mutex};
      ++shared_->stop_calls;
      shared_->changed.notify_all ();
    }

    void do_frequency (Frequency, MODE, bool) override
    {
      std::lock_guard<std::mutex> lock {shared_->mutex};
      ++shared_->frequency_writes;
      shared_->changed.notify_all ();
    }

    void do_tx_frequency (Frequency, MODE, bool) override
    {
      std::lock_guard<std::mutex> lock {shared_->mutex};
      ++shared_->tx_frequency_writes;
      shared_->changed.notify_all ();
    }

    void do_mode (MODE) override
    {
      std::lock_guard<std::mutex> lock {shared_->mutex};
      ++shared_->mode_writes;
      shared_->changed.notify_all ();
    }

    void do_ptt (bool on) override
    {
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->ptt_writes;
        shared_->changed.notify_all ();
      }
      update_PTT (on);
    }

    ReadOnlyRefreshResult do_read_only_refresh () override
    {
      if (ReadBehavior::unsupported == behavior_)
        return {ReadOnlyRefreshResult::unsupported};
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->get_calls;
        shared_->changed.notify_all ();
      }
      if (ReadBehavior::throws == behavior_)
        throw std::runtime_error {"deterministic fake GET exception"};
      if (ReadBehavior::api_failure == behavior_)
        {
          auto const was_known = state ().ptt_readback_known ();
          update_PTT_readback_known (false);
          return {ReadOnlyRefreshResult::api_failure, -17, true,
                  was_known, QStringLiteral ("rig_get_ptt")};
        }
      confirm_PTT_readback (state ().ptt ());
      return {ReadOnlyRefreshResult::api_success, 0, true, true,
              QStringLiteral ("rig_get_ptt")};
    }

    bool do_pre_update () override
    {
      std::lock_guard<std::mutex> lock {shared_->mutex};
      return !shared_->suppress_updates;
    }

  private:
    std::shared_ptr<SharedState> shared_;
    ReadBehavior behavior_;
  };

  bool pump_until (QCoreApplication& app,
                   std::function<bool ()> const& ready,
                   char const * label, int timeout_ms = 5000)
  {
    QElapsedTimer timer;
    timer.start ();
    while (!ready () && timer.elapsed () < timeout_ms)
      {
        app.processEvents (QEventLoop::AllEvents, 20);
        QThread::msleep (1);
      }
    auto const ok = ready ();
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    return ok;
  }

  bool snapshot_ready (std::shared_ptr<SharedState> const& shared,
                      std::function<bool (SharedState const&)> predicate)
  {
    std::lock_guard<std::mutex> lock {shared->mutex};
    return predicate (*shared);
  }

  FakeRig * create_rig (QThread& worker, QCoreApplication& app,
                        std::shared_ptr<SharedState> const& shared,
                        ReadBehavior behavior, QString const& diagnostics)
  {
    auto rig = new FakeRig {shared, behavior, diagnostics};
    rig->moveToThread (&worker);
    QObject::connect (rig, &Transceiver::update, &app,
      [shared] (Transceiver::TransceiverState const& state, unsigned) {
        std::lock_guard<std::mutex> lock {shared->mutex};
        ++shared->update_calls;
        shared->update_online = state.online ();
        shared->update_ptt_known = state.ptt_readback_known ();
        shared->update_ptt = state.ptt ();
        shared->update_serial = state.ptt_readback_serial ();
        shared->changed.notify_all ();
      }, Qt::QueuedConnection);
    QObject::connect (rig, &Transceiver::read_only_refresh_completed, &app,
      [shared] (quint64, qint64, int status, QString const&, int, bool,
                unsigned sequence, quint64 serial) {
        std::lock_guard<std::mutex> lock {shared->mutex};
        ++shared->refresh_events;
        shared->refresh_status = status;
        shared->refresh_sequence = sequence;
        shared->refresh_serial = serial;
        shared->changed.notify_all ();
      }, Qt::QueuedConnection);
    QObject::connect (rig, &Transceiver::finished,
                      rig, &QObject::deleteLater, Qt::QueuedConnection);
    QObject::connect (&worker, &QThread::finished,
                      rig, &QObject::deleteLater);
    QObject::connect (rig, &QObject::destroyed, &app,
      [shared] {
        std::lock_guard<std::mutex> lock {shared->mutex};
        shared->destroyed = true;
        shared->changed.notify_all ();
      }, Qt::QueuedConnection);
    return rig;
  }

  bool no_writes (std::shared_ptr<SharedState> const& shared)
  {
    return snapshot_ready (shared, [] (SharedState const& s) {
      return s.frequency_writes == 0 && s.tx_frequency_writes == 0
        && s.mode_writes == 0 && s.ptt_writes == 0;
    });
  }

  bool run_active_success (QCoreApplication& app, QThread& worker,
                           QString const& diagnostics)
  {
    auto shared = std::make_shared<SharedState> ();
    auto rig = create_rig (worker, app, shared, ReadBehavior::success, diagnostics);
    bool ok = QMetaObject::invokeMethod (rig, [rig] {
      rig->refresh_read_only (1);
    }, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.refresh_events == 1;
      });
    }, "inactive refresh returns without opening or reading") && ok;
    ok = snapshot_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 0 && s.get_calls == 0
        && s.refresh_status == Transceiver::ReadOnlyRefreshResult::inactive;
    }) && ok;

    ok = QMetaObject::invokeMethod (rig, [rig] {rig->start (51, nullptr);},
                                    Qt::QueuedConnection) && ok;
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.start_calls == 1 && s.update_online
          && s.update_ptt_known && !s.update_ptt;
      });
    }, "production TransceiverBase starts fake rig and establishes known PTT") && ok;
    int updates_before_refresh;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->suppress_updates = true;
      updates_before_refresh = shared->update_calls;
    }
    ok = QMetaObject::invokeMethod (rig, [rig] {
      rig->refresh_read_only (2);
    }, Qt::QueuedConnection) && ok;
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.refresh_events == 2;
      });
    }, "active read-only refresh invokes exactly one fake GET") && ok;
    ok = snapshot_ready (shared, [updates_before_refresh] (SharedState const& s) {
      return s.refresh_status == Transceiver::ReadOnlyRefreshResult::api_success
        && s.get_calls == 1 && s.start_calls == 1
        && s.refresh_sequence == 51 && s.refresh_serial > 1
        && s.update_calls == updates_before_refresh
        && s.update_serial == 1;
    }) && ok;
    ok = no_writes (shared) && ok;

    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->suppress_updates = false;
    }
    QMetaObject::invokeMethod (rig, [rig] {
      rig->stop ();
      rig->refresh_read_only (3);
    }, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.destroyed && s.refresh_events == 3;
      });
    }, "retired supported worker rejects a post-stop refresh before deletion") && ok;
    ok = snapshot_ready (shared, [] (SharedState const& s) {
      return s.refresh_status == Transceiver::ReadOnlyRefreshResult::inactive
        && s.get_calls == 1 && s.start_calls == 1;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "active refresh preserves control sequence and issues no driver writes/start\n";
    return ok;
  }

  bool run_new_object_recovery (QCoreApplication& app, QThread& worker,
                               QString const& diagnostics)
  {
    auto shared = std::make_shared<SharedState> ();
    auto rig = create_rig (worker, app, shared, ReadBehavior::success, diagnostics);
    bool ok = QMetaObject::invokeMethod (rig, [rig] {rig->start (81, nullptr);},
                                        Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.start_calls == 1 && s.update_online && s.update_ptt_known;
      });
    }, "fresh TransceiverBase object starts after prior object retirement") && ok;
    ok = QMetaObject::invokeMethod (rig, [rig] {rig->refresh_read_only (4);},
                                    Qt::QueuedConnection) && ok;
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.refresh_events == 1;
      });
    }, "new object accepts active refresh after its own startup") && ok;
    ok = snapshot_ready (shared, [] (SharedState const& s) {
      return s.refresh_status == Transceiver::ReadOnlyRefreshResult::api_success
        && s.get_calls == 1 && s.start_calls == 1 && s.refresh_sequence == 81;
    }) && ok;
    ok = no_writes (shared) && ok;
    QMetaObject::invokeMethod (rig, [rig] {rig->stop ();}, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {return s.destroyed;});
    }, "new active object stops and is deleted") && ok;
    return ok;
  }

  bool run_failure_and_exception (QCoreApplication& app, QThread& worker,
                                  QString const& diagnostics,
                                  ReadBehavior behavior, int expected_status,
                                  char const * label)
  {
    auto shared = std::make_shared<SharedState> ();
    auto rig = create_rig (worker, app, shared, behavior, diagnostics);
    bool ok = QMetaObject::invokeMethod (rig, [rig] {rig->start (61, nullptr);},
                                        Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.start_calls == 1 && s.update_online && s.update_ptt_known;
      });
    }, "failure fixture is active with known PTT") && ok;
    ok = QMetaObject::invokeMethod (rig, [rig] {rig->refresh_read_only (9);},
                                    Qt::QueuedConnection) && ok;
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {
        return s.refresh_events == 1 && !s.update_ptt_known;
      });
    }, label) && ok;
    ok = snapshot_ready (shared, [expected_status] (SharedState const& s) {
      return s.refresh_status == expected_status && s.get_calls == 1
        && s.start_calls == 1 && !s.update_ptt_known;
    }) && ok;
    ok = no_writes (shared) && ok;
    QMetaObject::invokeMethod (rig, [rig] {rig->stop ();}, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (shared, [] (SharedState const& s) {return s.destroyed;});
    }, "failed refresh worker still terminates safely") && ok;
    return ok;
  }

  bool run_unsupported_and_retired (QCoreApplication& app, QThread& worker,
                                    QString const& diagnostics)
  {
    auto unsupported = std::make_shared<SharedState> ();
    auto unsupported_rig = create_rig (worker, app, unsupported,
                                       ReadBehavior::unsupported, diagnostics);
    bool ok = QMetaObject::invokeMethod (unsupported_rig,
      [unsupported_rig] {unsupported_rig->start (71, nullptr);}, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (unsupported, [] (SharedState const& s) {
        return s.start_calls == 1 && s.update_online;
      });
    }, "unsupported backend fixture starts normally") && ok;
    ok = QMetaObject::invokeMethod (unsupported_rig,
      [unsupported_rig] {unsupported_rig->refresh_read_only (10);},
      Qt::QueuedConnection) && ok;
    ok = pump_until (app, [&] {
      return snapshot_ready (unsupported, [] (SharedState const& s) {
        return s.refresh_events == 1;
      });
    }, "unsupported backend returns unsupported without issuing GET") && ok;
    ok = snapshot_ready (unsupported, [] (SharedState const& s) {
      return s.refresh_status == Transceiver::ReadOnlyRefreshResult::unsupported
        && s.get_calls == 0 && s.start_calls == 1;
    }) && ok;
    QMetaObject::invokeMethod (unsupported_rig, [unsupported_rig] {
      unsupported_rig->stop ();
      unsupported_rig->refresh_read_only (11);
    }, Qt::QueuedConnection);
    ok = pump_until (app, [&] {
      return snapshot_ready (unsupported, [] (SharedState const& s) {return s.destroyed;});
    }, "retired worker terminates after queued refresh") && ok;
    ok = snapshot_ready (unsupported, [] (SharedState const& s) {
      return s.get_calls == 0 && s.start_calls == 1;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "unsupported and retired sessions cannot perform hidden reads\n";
    return ok;
  }
}

int main (int argc, char * argv[])
{
  if (argc < 2)
    {
      std::cerr << "FAIL missing test evidence directory\n";
      return 2;
    }
  auto const evidence_dir = QDir::cleanPath (QString::fromLocal8Bit (argv[1]));
  if (!QDir {}.mkpath (evidence_dir))
    {
      std::cerr << "FAIL cannot create test evidence directory\n";
      return 2;
    }
  QCoreApplication app {argc, argv};
  app.setOrganizationName (QStringLiteral ("JTDX"));
  app.setApplicationName (QStringLiteral ("TransceiverReadonlyRefreshTest"));
  std::cout << "qt_data_location_read_only="
            << QStandardPaths::writableLocation (QStandardPaths::DataLocation).toLocal8Bit ().constData ()
            << '\n' << "injected_failure_diagnostic_directory="
            << evidence_dir.toLocal8Bit ().constData () << '\n';

  QThread worker;
  worker.start ();
  auto const diagnostics = evidence_dir + QStringLiteral ("/worker-diagnostics");
  bool ok = run_active_success (app, worker, diagnostics);
  ok = run_new_object_recovery (app, worker, diagnostics) && ok;
  ok = run_failure_and_exception (
         app, worker, diagnostics, ReadBehavior::api_failure,
         Transceiver::ReadOnlyRefreshResult::api_failure,
         "API failure refresh invalidates previously known PTT") && ok;
  ok = run_failure_and_exception (
         app, worker, diagnostics, ReadBehavior::throws,
         Transceiver::ReadOnlyRefreshResult::internal_failure,
         "thrown GET invalidates known PTT and emits internal_failure") && ok;
  ok = run_unsupported_and_retired (app, worker, diagnostics) && ok;
  worker.quit ();
  auto const worker_stopped = worker.wait (5000);
  std::cout << (worker_stopped ? "PASS " : "FAIL ")
            << "worker QThread exits within bounded timeout\n";
  return worker_stopped && ok ? 0 : 1;
}
