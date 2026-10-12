#include "TransceiverBase.hpp"
#include "Transceiver.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaType>
#include <QMetaObject>
#include <QThread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
  std::atomic<bool> unregistered_token_warning {false};

  void qt_message_handler (QtMsgType type, QMessageLogContext const&,
                          QString const& message)
  {
    if (type == QtWarningMsg
        && message.contains (QStringLiteral ("PttAdmissionBlock::Token")))
      unregistered_token_warning.store (true);
    std::cerr << (type == QtWarningMsg ? "QT_WARNING: " : "QT_MESSAGE: ")
              << message.toLocal8Bit ().constData () << std::endl;
  }

  bool wait_until (std::function<bool ()> predicate)
  {
    auto const deadline = std::chrono::steady_clock::now ()
      + std::chrono::seconds (3);
    while (std::chrono::steady_clock::now () < deadline)
      {
        if (predicate ()) return true;
        std::this_thread::sleep_for (std::chrono::milliseconds (2));
      }
    return predicate ();
  }

  QString read_source (QString const& relative)
  {
    QFile file {QStringLiteral (JTDX_SOURCE_DIR "/") + relative};
    if (!file.open (QIODevice::ReadOnly)) return {};
    auto text = QString::fromUtf8 (file.readAll ());
    text.replace (QStringLiteral ("\r\n"), QStringLiteral ("\n"));
    return text;
  }

  bool production_registration_path_is_wired ()
  {
    auto const registry = read_source (QStringLiteral ("MetaDataRegistry.cpp"));
    auto const register_body = registry.indexOf (
      QStringLiteral ("void register_types ()"));
    auto const register_call = registry.indexOf (
      QStringLiteral ("register_transceiver_meta_types ();"), register_body);
    auto const transceiver = read_source (QStringLiteral ("Transceiver.cpp"));
    auto const token_registration = transceiver.indexOf (
      QStringLiteral ("qRegisterMetaType<PttAdmissionBlock::Token> (\"PttAdmissionBlock::Token\")"));
    auto const application = read_source (QStringLiteral ("main.cpp"));
    auto const application_registration = application.indexOf (
      QStringLiteral ("register_types ();"));
    auto const main_window = application.indexOf (QStringLiteral ("MainWindow w"));
    return register_body >= 0 && register_call > register_body
      && token_registration >= 0 && application_registration >= 0
      && main_window > application_registration;
  }
}

class ConfigurationEmitAdapter final : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;
  std::shared_ptr<PttAdmissionBlock> ptt_admission_block_;
  Transceiver::TransceiverState cached_rig_state_;
  unsigned transceiver_command_number_ {9};

  void send (Transceiver::TransceiverState const& state)
  {
    cached_rig_state_ = state;
    emit_transceiver_state ();
  }

  void emit_transceiver_state ();

Q_SIGNALS:
  void set_transceiver (Transceiver::TransceiverState const& state,
                        unsigned sequence,
                        PttAdmissionBlock::Token token);
};

class BaseFake final : public TransceiverBase
{
public:
  explicit BaseFake (QString const& evidence_dir)
    : TransceiverBase {nullptr, evidence_dir} {}

  std::atomic<int> start_calls {0};
  std::atomic<int> frequency_calls {0};
  std::atomic<int> ptt_on_calls {0};
  std::atomic<int> ptt_off_calls {0};
  std::atomic<int> failures {0};
  std::atomic<unsigned> last_frequency_sequence {0};
  std::atomic<unsigned> last_ptt_sequence {0};

  void block_next_frequency ()
  {
    std::lock_guard<std::mutex> lock {mutex_};
    block_frequency_ = true;
    frequency_entered_ = false;
    release_frequency_ = false;
  }

  bool wait_frequency_entered ()
  {
    std::unique_lock<std::mutex> lock {mutex_};
    return changed_.wait_for (lock, std::chrono::seconds (3), [this] {
      return frequency_entered_;
    });
  }

  void release_frequency ()
  {
    std::lock_guard<std::mutex> lock {mutex_};
    release_frequency_ = true;
    changed_.notify_all ();
  }

protected:
  int do_start (JTDXDateTime*) override
  {
    ++start_calls;
    return 1;
  }

  void do_stop () override {}

  void do_frequency (Frequency frequency, MODE mode, bool) override
  {
    last_frequency_sequence.store (current_control_sequence ());
    ++frequency_calls;
    {
      std::unique_lock<std::mutex> lock {mutex_};
      if (block_frequency_)
        {
          block_frequency_ = false;
          frequency_entered_ = true;
          changed_.notify_all ();
          changed_.wait (lock, [this] {return release_frequency_;});
        }
    }
    update_rx_frequency (frequency);
    if (mode != UNK) update_mode (mode);
  }

  void do_tx_frequency (Frequency frequency, MODE mode, bool) override
  {
    update_other_frequency (frequency);
    update_split (frequency != 0);
    if (mode != UNK) update_mode (mode);
  }

  void do_mode (MODE mode) override {update_mode (mode);}

  void do_ptt (bool on) override
  {
    last_ptt_sequence.store (current_control_sequence ());
    if (on) ++ptt_on_calls;
    else ++ptt_off_calls;
    update_PTT (on);
  }

private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool block_frequency_ {false};
  bool frequency_entered_ {false};
  bool release_frequency_ {false};
};

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  qInstallMessageHandler (qt_message_handler);
  if (argc < 2)
    {
      std::cerr << "expected repository-local evidence directory\n";
      return 2;
    }
  QString const evidence_dir = QString::fromLocal8Bit (argv[1]);
  if (!QDir {evidence_dir}.mkpath (QStringLiteral ("."))) return 2;

  // Use the same lightweight production registration routine called by
  // MetaDataRegistry::register_types; this fixture never registers the token.
  register_transceiver_meta_types ();
  bool ok = QMetaType::type ("PttAdmissionBlock::Token") != QMetaType::UnknownType;
  ok = production_registration_path_is_wired () && ok;

  QThread worker;
  auto admission = std::make_shared<PttAdmissionBlock> ();
  auto * receiver = new BaseFake {evidence_dir};
  receiver->bind_ptt_admission (admission);
  receiver->move_to_thread_tree (&worker);
  worker.start ();
  QObject::connect (receiver, &Transceiver::failure, &app,
                    [receiver] (QString const&) {++receiver->failures;},
                    Qt::DirectConnection);

  ConfigurationEmitAdapter sender;
  sender.ptt_admission_block_ = admission;
  bool const connected = QObject::connect (
    &sender, &ConfigurationEmitAdapter::set_transceiver,
    receiver, &Transceiver::set); // typed AutoConnection across QThread
  ok = connected && ok;

  auto state = Transceiver::TransceiverState {};
  state.online (true);
  state.frequency (14074000);
  state.mode (Transceiver::USB);
  sender.send (state);
  ok = wait_until ([receiver] {
    return receiver->start_calls.load () == 1
      && receiver->frequency_calls.load () == 1
      && receiver->last_frequency_sequence.load () == 10;
  }) && ok;

  admission->grant ();
  state.ptt (true);
  sender.send (state);
  ok = wait_until ([receiver] {
    return receiver->ptt_on_calls.load () == 1
      && receiver->last_ptt_sequence.load () == 11;
  }) && ok;

  state.ptt (false);
  sender.send (state);
  ok = wait_until ([receiver] {
    return receiver->ptt_off_calls.load () == 1
      && receiver->last_ptt_sequence.load () == 12;
  }) && ok;

  // Hold the worker inside a normal frequency update, queue an ON using the
  // old token through the real signal, revoke it, then let the worker drain.
  state.frequency (14075000);
  receiver->block_next_frequency ();
  sender.send (state);
  bool const entered = receiver->wait_frequency_entered ();
  ok = entered && ok;
  auto stale_on = state;
  stale_on.frequency (14076000);
  stale_on.ptt (true);
  sender.send (stale_on);
  admission->revoke ();
  receiver->release_frequency ();
  ok = wait_until ([receiver] {
    return receiver->frequency_calls.load () == 4
      && receiver->last_frequency_sequence.load () == 14;
  }) && ok;
  std::this_thread::sleep_for (std::chrono::milliseconds (30));
  ok = receiver->ptt_on_calls.load () == 1
    && receiver->ptt_off_calls.load () == 1
    && receiver->failures.load () == 0 && ok;

  admission->grant ();
  sender.send (stale_on);
  ok = wait_until ([receiver] {
    return receiver->ptt_on_calls.load () == 2
      && receiver->last_ptt_sequence.load () == 15;
  }) && ok;
  stale_on.ptt (false);
  sender.send (stale_on);
  ok = wait_until ([receiver] {
    return receiver->ptt_off_calls.load () == 2
      && receiver->last_ptt_sequence.load () == 16;
  }) && ok;

  ok = !unregistered_token_warning.load () && ok;
  std::cout << "token_meta_id="
            << QMetaType::type ("PttAdmissionBlock::Token")
            << " start=" << receiver->start_calls.load ()
            << " frequency=" << receiver->frequency_calls.load ()
            << " ptt_on=" << receiver->ptt_on_calls.load ()
            << " ptt_off=" << receiver->ptt_off_calls.load ()
            << " failures=" << receiver->failures.load () << '\n';
  std::cout << (ok ? "PASS" : "FAIL")
            << " production Configuration emitter reaches typed AutoConnection; revoked queued ON is refused and a fresh grant works\n";

  bool const stopped = QMetaObject::invokeMethod (
    receiver, "stop", Qt::BlockingQueuedConnection);
  worker.quit ();
  bool const joined = worker.wait (3000);
  delete receiver;
  return ok && stopped && joined ? 0 : 1;
}

#include "transceiver_command_delivery_test.moc"
