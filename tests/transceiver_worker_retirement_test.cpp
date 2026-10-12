#include "TransceiverBase.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QDir>
#include <QFile>

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
  struct SharedState
  {
    std::mutex mutex;
    std::condition_variable changed;
    int start_calls {0};
    int stop_calls {0};
    int ptt_on_calls {0};
    int ptt_off_calls {0};
    bool ptt_off_seen_before_stop {false};
    bool throw_next_ptt_off {false};
    bool inspect_backend_log_during_post_stop {false};
    int backend_return_count_during_post_stop {-1};
    int post_stop_calls {0};
    int fail_stop_attempts {0};
    bool block_next_start {false};
    bool fail_next_start {false};
    bool release_start {false};
    bool start_entered {false};
    bool block_next_stop {false};
    bool release_stop {false};
    bool stop_entered {false};
    bool fault_entered {false};
    bool release_fault {false};
    bool block_next_worker_operation {false};
    bool worker_operation_entered {false};
    bool release_worker_operation {false};
    bool block_next_tx_frequency {false};
    bool tx_frequency_entered {false};
    bool release_tx_frequency {false};
    int tx_frequency_calls {0};
    int tx_frequency_about_to_tx_calls {0};
    int rx_frequency_calls {0};
    int mode_calls {0};
    Transceiver::Frequency rx_frequency {0};
    Transceiver::Frequency tx_frequency {0};
    bool split {false};
    bool ptt_on {false};
    Transceiver::MODE mode {Transceiver::UNK};
    std::shared_ptr<PttAdmissionBlock> admission;
  };

  std::vector<std::shared_ptr<SharedState>> all_shared_states;
  QString failure_diagnostic_directory;

  QString read_diagnostic_log ()
  {
    QFile file {QDir {failure_diagnostic_directory}.filePath (
      QStringLiteral ("jtdx_recovery.log"))};
    if (!file.open (QIODevice::ReadOnly)) return {};
    return QString::fromUtf8 (file.readAll ());
  }

  int event_count (QString const& log, QString const& event)
  {
    return log.count (event);
  }

  QString event_line (QString const& log, QString const& event,
                      QString const& sequence)
  {
    for (auto const& line : log.split (QLatin1Char ('\n')))
      if (line.contains (event) && line.contains (sequence)) return line;
    return {};
  }

  class FakeRig final : public TransceiverBase
  {
  public:
    explicit FakeRig (std::shared_ptr<SharedState> shared)
      : TransceiverBase {nullptr, failure_diagnostic_directory}
      , shared_ {std::move (shared)}
    {
      set_diagnostic_session_generation (97);
    }

    void block_then_fail ()
    {
      {
        std::unique_lock<std::mutex> lock {shared_->mutex};
        shared_->fault_entered = true;
        shared_->changed.notify_all ();
        shared_->changed.wait (lock, [this] {return shared_->release_fault;});
      }
      offline (QStringLiteral ("deterministic fake worker failure"));
    }

    void report_offline_twice ()
    {
      offline (QStringLiteral ("deterministic first fake failure"));
      offline (QStringLiteral ("deterministic repeated fake failure"));
    }

    void block_worker_operation ()
    {
      std::unique_lock<std::mutex> lock {shared_->mutex};
      shared_->worker_operation_entered = true;
      shared_->changed.notify_all ();
      shared_->changed.wait (lock, [this] {return shared_->release_worker_operation;});
    }

  protected:
    void do_post_stop () override
    {
      bool inspect;
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->post_stop_calls;
        inspect = shared_->inspect_backend_log_during_post_stop;
      }
      if (inspect)
        {
          auto const count = event_count (
            read_diagnostic_log (),
            QStringLiteral ("stop_diag event=shutdown_backend_stop"));
          std::lock_guard<std::mutex> lock {shared_->mutex};
          shared_->backend_return_count_during_post_stop = count;
        }
    }

    int do_start (JTDXDateTime*) override
    {
      bool block;
      bool fail;
      {
        std::unique_lock<std::mutex> lock {shared_->mutex};
        ++shared_->start_calls;
        shared_->start_entered = true;
        block = shared_->block_next_start;
        shared_->block_next_start = false;
        fail = shared_->fail_next_start;
        shared_->fail_next_start = false;
        shared_->changed.notify_all ();
        if (block)
          shared_->changed.wait (lock, [this] {return shared_->release_start;});
      }
      if (fail) throw std::runtime_error {"deterministic fake startup failure"};
      return 1;
    }

    void do_stop () override
    {
      bool fail;
      {
        std::unique_lock<std::mutex> lock {shared_->mutex};
        shared_->ptt_off_seen_before_stop |= shared_->ptt_off_calls > 0;
        ++shared_->stop_calls;
        fail = shared_->fail_stop_attempts > 0;
        if (fail) --shared_->fail_stop_attempts;
        if (shared_->block_next_stop)
          {
            shared_->block_next_stop = false;
            shared_->stop_entered = true;
            shared_->changed.notify_all ();
            shared_->changed.wait (lock, [this] {return shared_->release_stop;});
          }
        shared_->changed.notify_all ();
      }
      if (fail) throw std::runtime_error {"deterministic fake stop failure"};
    }

    void do_frequency (Frequency frequency, MODE mode, bool) override
    {
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->rx_frequency_calls;
        shared_->rx_frequency = frequency;
        if (mode != UNK) shared_->mode = mode;
        shared_->changed.notify_all ();
      }
      update_rx_frequency (frequency);
      if (mode != UNK) update_mode (mode);
    }
    void do_tx_frequency (Frequency frequency, MODE mode, bool about_to_tx) override
    {
      {
        std::unique_lock<std::mutex> lock {shared_->mutex};
        ++shared_->tx_frequency_calls;
        if (about_to_tx) ++shared_->tx_frequency_about_to_tx_calls;
        shared_->tx_frequency = frequency;
        shared_->split = frequency != 0;
        if (mode != UNK)
          {
            shared_->mode = mode;
            ++shared_->mode_calls;
          }
        shared_->changed.notify_all ();
        if (shared_->block_next_tx_frequency)
          {
            shared_->block_next_tx_frequency = false;
            shared_->tx_frequency_entered = true;
            shared_->changed.notify_all ();
            shared_->changed.wait (lock, [this] {return shared_->release_tx_frequency;});
          }
      }
      update_other_frequency (frequency);
      update_split (frequency != 0);
      if (mode != UNK) update_mode (mode);
    }
    void do_mode (MODE mode) override
    {
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        ++shared_->mode_calls;
        shared_->mode = mode;
        shared_->changed.notify_all ();
      }
      update_mode (mode);
    }

    void do_ptt (bool on) override
    {
      bool fail = false;
      {
        std::lock_guard<std::mutex> lock {shared_->mutex};
        if (on) ++shared_->ptt_on_calls;
        else ++shared_->ptt_off_calls;
        if (!on && shared_->throw_next_ptt_off)
          {
            shared_->throw_next_ptt_off = false;
            fail = true;
          }
        else shared_->ptt_on = on;
        shared_->changed.notify_all ();
      }
      if (fail) throw std::runtime_error {"deterministic fake PTT OFF failure"};
      update_PTT (on);
    }

  private:
    std::shared_ptr<SharedState> shared_;
  };

  struct GuiRecovery
  {
    int failure_count {0};
    bool stop_queued {false};
  };

  bool enqueue (QObject * target, std::function<void ()> call)
  {
    return QMetaObject::invokeMethod (
      target, [call] {call ();}, Qt::QueuedConnection);
  }

  bool pump_until (QCoreApplication& app,
                   std::function<bool ()> const& ready,
                   char const * label,
                   int timeout_ms = 5000)
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

  template <typename Predicate>
  bool shared_ready (std::shared_ptr<SharedState> const& shared,
                     Predicate predicate)
  {
    std::lock_guard<std::mutex> lock {shared->mutex};
    return predicate (*shared);
  }

  FakeRig * make_rig (QThread& worker,
                      std::shared_ptr<SharedState> const& shared,
                      QCoreApplication& app,
                      bool& destroyed)
  {
    auto rig = new FakeRig {shared};
    shared->admission = std::make_shared<PttAdmissionBlock> ();
    rig->bind_ptt_admission (shared->admission);
    rig->moveToThread (&worker);
    QObject::connect (rig, &Transceiver::finished,
                      rig, &QObject::deleteLater, Qt::QueuedConnection);
    QObject::connect (&worker, &QThread::finished,
                      rig, &QObject::deleteLater);
    QObject::connect (rig, &QObject::destroyed, &app,
                      [&destroyed] {destroyed = true;}, Qt::QueuedConnection);
    return rig;
  }

  void connect_failure_to_late_stop (FakeRig * rig,
                                    QCoreApplication& app,
                                    GuiRecovery& recovery)
  {
    QObject::connect (rig, &Transceiver::failure, &app,
      [rig, &recovery] (QString const&) {
        ++recovery.failure_count;
        recovery.stop_queued = enqueue (rig, [rig] {rig->stop ();});
      }, Qt::QueuedConnection);
  }

  void connect_failure_counter (FakeRig * rig,
                                QCoreApplication& app,
                                GuiRecovery& recovery)
  {
    QObject::connect (rig, &Transceiver::failure, &app,
      [&recovery] (QString const&) {++recovery.failure_count;},
      Qt::QueuedConnection);
  }

  Transceiver::TransceiverState state (bool online, bool ptt)
  {
    Transceiver::TransceiverState value;
    value.online (online);
    value.ptt (ptt);
    return value;
  }

  Transceiver::TransceiverState control_state (bool ptt,
                                               Transceiver::Frequency rx_frequency,
                                               Transceiver::Frequency tx_frequency,
                                               Transceiver::MODE mode)
  {
    auto value = state (true, ptt);
    value.frequency (rx_frequency);
    value.tx_frequency (tx_frequency);
    value.mode (mode);
    return value;
  }

  bool run_queued_ptt_admission_race (QCoreApplication& app, QThread& worker)
  {
    auto old_session = std::make_shared<SharedState> ();
    auto new_session = std::make_shared<SharedState> ();
    all_shared_states.push_back (old_session);
    all_shared_states.push_back (new_session);
    bool old_destroyed {false};
    bool new_destroyed {false};
    GuiRecovery old_recovery;
    GuiRecovery new_recovery;
    auto old_rig = make_rig (worker, old_session, app, old_destroyed);
    auto new_rig = make_rig (worker, new_session, app, new_destroyed);
    connect_failure_counter (old_rig, app, old_recovery);
    connect_failure_counter (new_rig, app, new_recovery);

    bool ok = enqueue (old_rig, [old_rig] {old_rig->start (0xfffffffdU, nullptr);});
    ok = enqueue (new_rig, [new_rig] {new_rig->start (0xfffffffdU, nullptr);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (old_session, [] (SharedState const& s) {return s.start_calls == 1;})
        && shared_ready (new_session, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "both session-scoped admission fakes start") && ok;

    auto baseline = control_state (false, 14074000, 0, Transceiver::USB);
    ok = enqueue (old_rig, [old_rig, baseline] {
      old_rig->set (baseline, 0xfffffffdU, 0);
    }) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (old_session, [] (SharedState const& s) {
        return s.rx_frequency_calls == 1 && s.rx_frequency == 14074000;
      });
    }, "RX baseline is established before queued stale PTT") && ok;

    auto old_token = old_session->admission->grant ();
    ok = enqueue (old_rig, [old_rig] {old_rig->block_worker_operation ();}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (old_session, [] (SharedState const& s) {
        return s.worker_operation_entered;
      });
    }, "ordinary worker operation blocks before queued PTT state") && ok;

    auto stale_on = control_state (true, 0, 14076000, Transceiver::DIG_U);
    ok = enqueue (old_rig, [old_rig, stale_on, old_token] {
      old_rig->set (stale_on, 0xfffffffeU, old_token);
    }) && ok;
    old_session->admission->revoke ();
    auto fresh_token = old_session->admission->grant ();
    auto fresh_on = control_state (true, 0, 14076000, Transceiver::DIG_U);
    auto normal_off = control_state (false, 0, 14076000, Transceiver::DIG_U);
    auto split_off = control_state (false, 0, 0, Transceiver::USB);
    ok = enqueue (old_rig, [old_rig, fresh_on, fresh_token] {
      old_rig->set (fresh_on, 0xffffffffU, fresh_token);
    }) && ok;
    ok = enqueue (old_rig, [old_rig, normal_off] {
      old_rig->set (normal_off, 0U, 0);
    }) && ok;
    ok = enqueue (old_rig, [old_rig, split_off] {
      old_rig->set (split_off, 1U, 0);
    }) && ok;

    // A distinct session has its own block. Revoking the old session neither
    // admits its stale token there nor prevents a fresh token on this object.
    auto independent_token = new_session->admission->grant ();
    auto independent_on = control_state (true, 7074000, 7074000, Transceiver::LSB);
    auto independent_off = control_state (false, 7074000, 7074000, Transceiver::LSB);
    ok = enqueue (new_rig, [new_rig, independent_on, independent_token] {
      new_rig->set (independent_on, 0xfffffffeU, independent_token);
    }) && ok;
    ok = enqueue (new_rig, [new_rig, independent_off] {
      new_rig->set (independent_off, 0xffffffffU, 0);
    }) && ok;

    {
      std::lock_guard<std::mutex> lock {old_session->mutex};
      old_session->release_worker_operation = true;
      old_session->changed.notify_all ();
    }
    ok = pump_until (app, [&] {
      return shared_ready (old_session, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_off_calls == 1
          && s.tx_frequency_calls >= 2 && s.rx_frequency_calls >= 1;
      }) && shared_ready (new_session, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_off_calls == 1;
      });
    }, "revoked queued ON is suppressed, fresh-token ON/OFF and independent session remain usable") && ok;

    QString old_counts;
    {
      std::lock_guard<std::mutex> lock {old_session->mutex};
      auto const& s = *old_session;
      old_counts = QStringLiteral ("ptt_on=%1 ptt_off=%2 txfreq=%3 about_to_tx=%4 rxfreq=%5 txfreq_value=%6 split=%7 mode=%8")
        .arg (s.ptt_on_calls).arg (s.ptt_off_calls).arg (s.tx_frequency_calls)
        .arg (s.tx_frequency_about_to_tx_calls).arg (s.rx_frequency_calls)
        .arg (s.tx_frequency).arg (s.split).arg (static_cast<int> (s.mode));
    }
    QString new_counts;
    {
      std::lock_guard<std::mutex> lock {new_session->mutex};
      auto const& s = *new_session;
      new_counts = QStringLiteral ("ptt_on=%1 ptt_off=%2 txfreq=%3 about_to_tx=%4")
        .arg (s.ptt_on_calls).arg (s.ptt_off_calls).arg (s.tx_frequency_calls)
        .arg (s.tx_frequency_about_to_tx_calls);
    }
    std::cout << "admission-race-old " << old_counts.toLocal8Bit ().constData ()
              << "\nadmission-race-new " << new_counts.toLocal8Bit ().constData ()
              << " failures=" << old_recovery.failure_count << ','
              << new_recovery.failure_count << '\n';

    auto const diagnostic = read_diagnostic_log ();
    ok = diagnostic.contains (QStringLiteral (
      "ptt_on_suppressed sequence=4294967294 queued_token=%1 current_token=%2 stage=revoked_before_tx_frequency")
        .arg (old_token).arg (fresh_token)) && ok;
    ok = shared_ready (old_session, [] (SharedState const& s) {
      return s.ptt_on_calls == 1 && s.ptt_off_calls == 1 && !s.ptt_on
        && s.tx_frequency_calls >= 2 && s.tx_frequency_about_to_tx_calls == 1
        && s.rx_frequency == 14074000 && s.tx_frequency == 0 && !s.split
        && s.mode == Transceiver::USB && s.mode_calls > 0;
    }) && ok;
    ok = shared_ready (new_session, [] (SharedState const& s) {
      return s.ptt_on_calls == 1 && s.ptt_off_calls == 1 && !s.ptt_on;
    }) && ok;
    ok = old_recovery.failure_count == 0 && new_recovery.failure_count == 0 && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "old token stays blocked across command-sequence wrap; frequency/mode/split updates survive; new token and isolated session work\n";

    ok = enqueue (old_rig, [old_rig] {old_rig->stop ();}) && ok;
    ok = enqueue (new_rig, [new_rig] {new_rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return old_destroyed && new_destroyed;},
                     "both admission-test sessions stop cleanly") && ok;
    return ok;
  }

  bool run_revoke_preserves_existing_ptt (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_counter (rig, app, recovery);
    bool ok = enqueue (rig, [rig] {rig->start (2, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "normal PTT cancellation scenario starts production base") && ok;

    auto token = shared->admission->grant ();
    auto on = control_state (true, 14074000, 14074000, Transceiver::USB);
    ok = enqueue (rig, [rig, on, token] {rig->set (on, 3, token);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_on;
      });
    }, "explicit admission reaches the fake PTT driver") && ok;

    shared->admission->revoke ();
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.ptt_on_calls == 1 && s.ptt_off_calls == 0 && s.ptt_on;
    }) && ok;
    auto off = control_state (false, 14074000, 14074000, Transceiver::USB);
    ok = enqueue (rig, [rig, off] {rig->set (off, 4, 0);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.ptt_off_calls == 1 && !s.ptt_on;
      });
    }, "ordinary false state lowers previously active PTT after revoke") && ok;
    ok = recovery.failure_count == 0 && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "normal PTT cancellation scenario stops cleanly") && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "revocation itself does not lower an already active PTT; ordinary OFF remains available\n";
    return ok;
  }

  bool run_revoke_during_driver_operation (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_counter (rig, app, recovery);
    bool ok = enqueue (rig, [rig] {rig->start (5, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "in-flight driver cancellation scenario starts production base") && ok;

    auto const token = shared->admission->grant ();
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->block_next_tx_frequency = true;
    }
    auto on = control_state (true, 0, 14074000, Transceiver::USB);
    ok = enqueue (rig, [rig, on, token] {rig->set (on, 6, token);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.tx_frequency_entered;
      });
    }, "admitted ON reaches the fake TX-frequency driver before cancellation") && ok;
    shared->admission->revoke ();
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->release_tx_frequency = true;
      shared->changed.notify_all ();
    }
    auto off = control_state (false, 14075000, 0, Transceiver::USB);
    ok = enqueue (rig, [rig, off] {rig->set (off, 7, 0);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.rx_frequency_calls == 1;
      });
    }, "worker completes in-flight operation and proceeds to later state") && ok;
    auto const diagnostic = read_diagnostic_log ();
    ok = diagnostic.contains (QStringLiteral (
      "ptt_on_suppressed sequence=6 queued_token=%1 current_token=0 stage=revoked_before_ptt")
        .arg (token)) && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.tx_frequency_calls == 1 && s.tx_frequency_about_to_tx_calls == 1
        && s.ptt_on_calls == 0 && s.ptt_off_calls == 0 && !s.ptt_on
        && s.rx_frequency == 14075000 && s.tx_frequency == 14074000;
    }) && recovery.failure_count == 0 && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "revoke during an entered backend operation blocks final PTT call but cannot retract that operation\n";
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "in-flight driver cancellation scenario stops cleanly") && ok;
    return ok;
  }

  bool run_online_failure_race (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_to_late_stop (rig, app, recovery);

    bool ok = enqueue (rig, [rig] {rig->start (1, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "initial production TransceiverBase start reaches fake do_start") && ok;

    auto ptt_on = state (true, true);
    auto const firstToken = shared->admission->grant ();
    ok = enqueue (rig, [rig, ptt_on, firstToken] {rig->set (ptt_on, 2, firstToken);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.ptt_on_calls == 1;});
    }, "normal online PTT-on control reaches fake driver") && ok;
    shared->admission->revoke ();
    auto ptt_off = state (true, false);
    ok = enqueue (rig, [rig, ptt_off] {rig->set (ptt_off, 3, 0);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.ptt_off_calls >= 1;});
    }, "normal online PTT-off control reaches fake driver") && ok;

    ok = enqueue (rig, [rig] {rig->block_then_fail ();}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.fault_entered;});
    }, "fake worker fault operation is held before offline") && ok;

    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->block_next_stop = true;
    }
    auto stale_ptt_on = state (true, true);
    ok = enqueue (rig, [rig, stale_ptt_on, firstToken] {rig->set (stale_ptt_on, 4, firstToken);}) && ok;
    ok = enqueue (rig, [rig] {rig->start (5, nullptr);}) && ok;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->release_fault = true;
      shared->changed.notify_all ();
    }

    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.stop_entered;});
    }, "offline retires the session before failure cleanup blocks") && ok;
    ok = pump_until (app, [&] {return recovery.stop_queued;},
                     "GUI failure callback queues stop behind old set/start") && ok;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->release_stop = true;
      shared->changed.notify_all ();
    }
    ok = pump_until (app, [&] {return destroyed;},
                     "late stop finishes and deletes the retired worker") && ok;

    ok = recovery.stop_queued && ok;
    ok = recovery.failure_count == 1 && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 1 && s.ptt_on_calls == 1 && s.stop_calls == 2;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "old online/PTT set and queued start do not reopen or key after failure; cleanup ran once\n";
    return ok;
  }

  bool run_normal_offline_set (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    auto rig = make_rig (worker, shared, app, destroyed);
    GuiRecovery recovery;
    connect_failure_to_late_stop (rig, app, recovery);

    bool ok = enqueue (rig, [rig] {rig->start (10, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "normal offline scenario starts production base") && ok;
    auto offline = state (false, false);
    ok = enqueue (rig, [rig, offline] {rig->set (offline, 11, 0);}) && ok;
    auto online_ptt = state (true, true);
    ok = enqueue (rig, [rig, online_ptt] {rig->set (online_ptt, 12, 0);}) && ok;
    ok = enqueue (rig, [rig, online_ptt] {rig->set (online_ptt, 14, 0);}) && ok;
    ok = enqueue (rig, [rig] {rig->start (13, nullptr);}) && ok;
    ok = enqueue (rig, [rig] {rig->start (15, nullptr);}) && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "normal offline set retires before queued old commands and stop") && ok;
    ok = recovery.failure_count == 0 && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 1 && s.ptt_on_calls == 0 && s.stop_calls == 2;
    }) && ok;
    auto const retiredLog = read_diagnostic_log ();
    ok = retiredLog.contains (QStringLiteral (
             "retired_command_ignored action=set sequence=12 last_sequence=11 retired=true"))
      && retiredLog.contains (QStringLiteral (
             "retired_command_ignored action=start sequence=13 last_sequence=11 retired=true"))
      && !retiredLog.contains (QStringLiteral (
             "retired_command_ignored action=set sequence=14"))
      && !retiredLog.contains (QStringLiteral (
             "retired_command_ignored action=start sequence=15")) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "normal offline set/stop rejects later old session commands and records each retired action once\n";
    return ok;
  }

  bool run_repeated_offline_diagnostics (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_counter (rig, app, recovery);
    bool ok = enqueue (rig, [rig] {rig->start (16, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "repeated-offline scenario starts production base") && ok;
    auto const before = read_diagnostic_log ();
    auto const enteredEmitBefore = before.count (QStringLiteral (
      "transceiver_offline stage=entered failure_signal=will_emit_this_call"));
    auto const enteredPriorBefore = before.count (QStringLiteral (
      "transceiver_offline stage=entered failure_signal=already_emitted"));
    auto const afterEmitBefore = before.count (QStringLiteral (
      "transceiver_offline stage=after_emit failure_signal=emitted_this_call"));
    auto const afterPriorBefore = before.count (QStringLiteral (
      "transceiver_offline stage=after_emit failure_signal=already_emitted"));
    ok = enqueue (rig, [rig] {rig->report_offline_twice ();}) && ok;
    ok = pump_until (app, [&] {return recovery.failure_count == 1;},
                     "repeated offline failures emit only the first failure signal") && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "repeated-offline fake worker shuts down after its diagnostic check") && ok;
    auto const after = read_diagnostic_log ();
    ok = recovery.failure_count == 1
      && after.count (QStringLiteral (
           "transceiver_offline stage=entered failure_signal=will_emit_this_call"))
           == enteredEmitBefore + 1
      && after.count (QStringLiteral (
           "transceiver_offline stage=entered failure_signal=already_emitted"))
           == enteredPriorBefore + 1
      && after.count (QStringLiteral (
           "transceiver_offline stage=after_emit failure_signal=emitted_this_call"))
           == afterEmitBefore + 1
      && after.count (QStringLiteral (
           "transceiver_offline stage=after_emit failure_signal=already_emitted"))
           == afterPriorBefore + 1 && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "offline diagnostics truthfully distinguish this-call emission from an already emitted signal\n";
    return ok;
  }

  bool run_startup_failure (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_to_late_stop (rig, app, recovery);
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->block_next_start = true;
      shared->fail_next_start = true;
    }
    bool ok = enqueue (rig, [rig] {rig->start (20, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_entered;});
    }, "startup failure fake blocks inside production do_start") && ok;

    auto stale_ptt_on = state (true, true);
    ok = enqueue (rig, [rig, stale_ptt_on] {rig->set (stale_ptt_on, 21, 0);}) && ok;
    ok = enqueue (rig, [rig] {rig->start (22, nullptr);}) && ok;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->block_next_stop = true;
      shared->release_start = true;
      shared->changed.notify_all ();
    }
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.stop_entered;});
    }, "failed startup retires and attempts partial-start cleanup") && ok;
    ok = pump_until (app, [&] {return recovery.stop_queued;},
                     "startup failure is reported once and GUI queues stop") && ok;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->release_stop = true;
      shared->changed.notify_all ();
    }
    ok = pump_until (app, [&] {return destroyed;},
                     "startup-failed worker terminates after late stop") && ok;
    ok = recovery.failure_count == 1 && recovery.stop_queued && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 1 && s.ptt_on_calls == 0 && s.stop_calls == 2;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "startup failure cleanup runs and old commands cannot retry it\n";
    return ok;
  }

  bool run_new_object_recovery (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    auto rig = make_rig (worker, shared, app, destroyed);
    bool ok = enqueue (rig, [rig] {rig->start (30, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "fresh TransceiverBase object starts normally after retired object") && ok;
    auto ptt_on = state (true, true);
    auto const token = shared->admission->grant ();
    ok = enqueue (rig, [rig, ptt_on, token] {rig->set (ptt_on, 31, token);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.ptt_on_calls == 1;});
    }, "fresh object accepts normal PTT control") && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "fresh object stop terminates cleanly") && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 1 && s.ptt_on_calls == 1 && s.stop_calls == 2;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "new object owns a fresh session and retains normal operation\n";
    return ok;
  }

  bool run_stop_cleanup_exception (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_counter (rig, app, recovery);
    bool ok = enqueue (rig, [rig] {rig->start (40, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "cleanup-exception scenario starts production base") && ok;
    // Two cleanup attempts throw. The base must report failure only once,
    // finish the retired worker, and reject an old online/PTT command.
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->fail_stop_attempts = 2;
    }
    auto stale_ptt_on = state (true, true);
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = enqueue (rig, [rig, stale_ptt_on] {rig->set (stale_ptt_on, 41, 0);}) && ok;
    ok = enqueue (rig, [rig] {rig->start (42, nullptr);}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "cleanup error retries once then still terminates worker") && ok;
    ok = recovery.failure_count == 1 && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.start_calls == 1 && s.ptt_on_calls == 0 && s.stop_calls >= 3;
    }) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "do_stop exception is bounded to one failure and cannot revive retired worker\n";
    return ok;
  }

  bool run_runtime_ptt_off_diagnostic (QCoreApplication& app, QThread& worker,
                                      bool throw_driver)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    GuiRecovery recovery;
    auto rig = make_rig (worker, shared, app, destroyed);
    connect_failure_counter (rig, app, recovery);
    int const on_sequence = throw_driver ? 71 : 61;
    int const off_sequence = on_sequence + 1;
    bool ok = enqueue (rig, [rig, on_sequence] {rig->start (on_sequence, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "runtime OFF diagnostic scenario starts production worker") && ok;
    auto const token = shared->admission->grant ();
    auto const on_state = state (true, true);
    ok = enqueue (rig, [rig, on_state, on_sequence, token] {
      rig->set (on_state, on_sequence + 1, token);
    }) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_on;
      });
    }, "runtime OFF diagnostic scenario enters PTT ON") && ok;
    auto const log_before = read_diagnostic_log ();
    auto const off_event = QStringLiteral ("stop_diag event=ptt_off_driver");
    auto const off_events_before = event_count (log_before, off_event);
    if (throw_driver)
      {
        std::lock_guard<std::mutex> lock {shared->mutex};
        shared->throw_next_ptt_off = true;
      }
    auto const off_state = state (true, false);
    ok = enqueue (rig, [rig, off_state, off_sequence] {
      rig->set (off_state, off_sequence, 0);
    }) && ok;
    ok = pump_until (app, [&] {
      return event_count (read_diagnostic_log (), off_event) == off_events_before + 1;
    }, throw_driver ? "runtime PTT OFF exception emits its production driver result"
                    : "runtime PTT OFF return emits its production driver result") && ok;
    auto const log_after = read_diagnostic_log ();
    auto const line = event_line (
      log_after, off_event,
      QStringLiteral ("control_sequence=%1").arg (off_sequence));
    ok = !line.isEmpty ()
      && line.contains (QStringLiteral ("session_generation=97"))
      && line.contains (throw_driver
           ? QStringLiteral ("outcome=exception exception_type=std_exception")
           : QStringLiteral ("outcome=returned exception_type=none"))
      && line.contains (throw_driver
           ? QStringLiteral ("requested_online=0 cleanup_needed=0 reason=runtime_set")
           : QStringLiteral ("requested_online=1 cleanup_needed=1 reason=runtime_set"))
      && line.contains (QStringLiteral ("result_scope=driver_method_only readback=unknown"))
      && !line.contains (QStringLiteral ("endpoint="))
      && !line.contains (QStringLiteral ("callsign="))
      && !line.contains (QStringLiteral ("payload="))
      && !line.contains (QStringLiteral ("password=")) && ok;
    if (throw_driver)
      ok = pump_until (app, [&] {return recovery.failure_count == 1;},
                       "runtime PTT OFF exception preserves the original failure callback") && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "runtime OFF diagnostic worker terminates cleanly") && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << (throw_driver
                    ? "runtime OFF driver exception is logged without driver details\n"
                    : "runtime OFF driver return is distinct from unknown readback\n");
    return ok;
  }

  bool run_shutdown_requests_ptt_off (QCoreApplication& app, QThread& worker)
  {
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    auto rig = make_rig (worker, shared, app, destroyed);
    auto const token = shared->admission->grant ();
    auto const online_ptt = state (true, true);
    bool ok = enqueue (rig, [rig] {rig->start (50, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.start_calls == 1;
      });
    }, "shutdown OFF scenario starts the production TransceiverBase") && ok;
    ok = enqueue (rig, [rig, online_ptt, token] {
      rig->set (online_ptt, 51, token);
    }) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_on;
      });
    }, "fake backend confirms the production base entered PTT ON") && ok;
    auto const log_before_stop = read_diagnostic_log ();
    auto const off_events_before = event_count (
      log_before_stop, QStringLiteral ("stop_diag event=shutdown_ptt_off"));
    auto const backend_events_before = event_count (
      log_before_stop, QStringLiteral ("stop_diag event=shutdown_backend_stop"));
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->inspect_backend_log_during_post_stop = true;
    }
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "production stop retires the connected fake transceiver") && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.ptt_on_calls == 1 && s.ptt_off_calls == 1 && !s.ptt_on
        && s.ptt_off_seen_before_stop && s.stop_calls >= 1;
    }) && ok;
    auto const shutdown_log = read_diagnostic_log ();
    int backend_return_count_during_post_stop = -1;
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      backend_return_count_during_post_stop = shared->backend_return_count_during_post_stop;
    }
    ok = event_count (shutdown_log,
                      QStringLiteral ("stop_diag event=shutdown_ptt_off"))
           == off_events_before + 1
      && event_count (shutdown_log,
                      QStringLiteral ("stop_diag event=shutdown_backend_stop"))
           == backend_events_before + 1
      && backend_return_count_during_post_stop == backend_events_before
      && shutdown_log.contains (QStringLiteral (
           "stop_diag event=shutdown_ptt_off"))
      && shutdown_log.contains (QStringLiteral (
           "session_generation=97 control_sequence=51"))
      && shutdown_log.contains (QStringLiteral (
           "outcome=returned exception_type=none requested_online=1 cleanup_needed=1 reason=driver_call result_scope=driver_method_only readback=unknown"))
      && shutdown_log.contains (QStringLiteral (
           "stop_diag event=shutdown_backend_stop"))
      && shutdown_log.contains (QStringLiteral (
           "outcome=returned exception_type=none requested_online=1 cleanup_needed=1 reason=backend_stop_and_post_stop_returned"))
      && !shutdown_log.contains (QStringLiteral ("endpoint="))
      && !shutdown_log.contains (QStringLiteral ("callsign="))
      && !shutdown_log.contains (QStringLiteral ("payload="))
      && !shutdown_log.contains (QStringLiteral ("password=")) && ok;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "shutdown logs separate driver return and backend completion after post-stop, with unknown readback and bounded fields\n";
    return ok;
  }

  bool run_log_write_failure_preserves_shutdown (QCoreApplication& app,
                                                  QThread& worker,
                                                  QString const& blocker_path)
  {
    QFile blocker {blocker_path};
    if (!blocker.open (QIODevice::WriteOnly)) return false;
    blocker.close ();
    auto const previous_directory = failure_diagnostic_directory;
    failure_diagnostic_directory = blocker_path;
    auto shared = std::make_shared<SharedState> ();
    all_shared_states.push_back (shared);
    bool destroyed {false};
    auto rig = make_rig (worker, shared, app, destroyed);
    bool ok = enqueue (rig, [rig] {rig->start (80, nullptr);});
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {return s.start_calls == 1;});
    }, "blocked-log scenario starts production worker") && ok;
    auto const token = shared->admission->grant ();
    auto const ptt_on = state (true, true);
    ok = enqueue (rig, [rig, ptt_on, token] {rig->set (ptt_on, 81, token);}) && ok;
    ok = pump_until (app, [&] {
      return shared_ready (shared, [] (SharedState const& s) {
        return s.ptt_on_calls == 1 && s.ptt_on;
      });
    }, "blocked-log scenario enters PTT ON") && ok;
    ok = enqueue (rig, [rig] {rig->stop ();}) && ok;
    ok = pump_until (app, [&] {return destroyed;},
                     "blocked diagnostic writes do not prevent worker shutdown") && ok;
    ok = shared_ready (shared, [] (SharedState const& s) {
      return s.ptt_off_calls == 1 && !s.ptt_on && s.ptt_off_seen_before_stop
        && s.stop_calls >= 2 && s.post_stop_calls >= 2;
    }) && ok;
    failure_diagnostic_directory = previous_directory;
    std::cout << (ok ? "PASS " : "FAIL ")
              << "log write failures leave shutdown PTT OFF, backend stop, and post-stop sequence intact\n";
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
  auto evidence_dir = QDir::cleanPath (QString::fromLocal8Bit (argv[1]));
  if (!QDir {}.mkpath (evidence_dir))
    {
      std::cerr << "FAIL cannot create test evidence directory\n";
      return 2;
    }
  QCoreApplication app {argc, argv};
  app.setOrganizationName (QStringLiteral ("JTDX"));
  app.setApplicationName (QStringLiteral ("TransceiverWorkerRetirementTest"));
  failure_diagnostic_directory = evidence_dir;
  std::cout << "qt_data_location_read_only="
            << QStandardPaths::writableLocation (QStandardPaths::DataLocation).toLocal8Bit ().constData ()
            << '\n'
            << "injected_failure_diagnostic_directory="
            << failure_diagnostic_directory.toLocal8Bit ().constData () << '\n';
  QThread worker;
  worker.start ();

  bool ok = run_online_failure_race (app, worker);
  ok = run_queued_ptt_admission_race (app, worker) && ok;
  ok = run_revoke_preserves_existing_ptt (app, worker) && ok;
  ok = run_revoke_during_driver_operation (app, worker) && ok;
  ok = run_normal_offline_set (app, worker) && ok;
  ok = run_startup_failure (app, worker) && ok;
  ok = run_new_object_recovery (app, worker) && ok;
  ok = run_runtime_ptt_off_diagnostic (app, worker, false) && ok;
  ok = run_runtime_ptt_off_diagnostic (app, worker, true) && ok;
  ok = run_shutdown_requests_ptt_off (app, worker) && ok;
  ok = run_stop_cleanup_exception (app, worker) && ok;
  ok = run_repeated_offline_diagnostics (app, worker) && ok;
  ok = run_log_write_failure_preserves_shutdown (
         app, worker, QDir {evidence_dir}.filePath (QStringLiteral ("not-a-directory"))) && ok;

  // Release every deterministic wait even when an earlier assertion timed
  // out, so the test process never strands its worker thread.
  for (auto const& shared : all_shared_states)
    {
      std::lock_guard<std::mutex> lock {shared->mutex};
      shared->release_start = true;
      shared->release_stop = true;
      shared->release_fault = true;
      shared->release_worker_operation = true;
      shared->release_tx_frequency = true;
      shared->changed.notify_all ();
    }
  worker.quit ();
  auto const worker_stopped = worker.wait (5000);
  std::cout << (worker_stopped ? "PASS " : "FAIL ")
            << "worker QThread exits within bounded timeout\n";
  ok = worker_stopped && ok;
  return ok ? 0 : 1;
}
