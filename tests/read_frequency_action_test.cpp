/*
 * Source-body behavior harness for the A1 readFreq regression.
 * CMake appends exact current function bodies extracted from mainwindow.cpp
 * and Configuration.cpp. The adapter below models only the Configuration
 * calls and the transceiver_tx_frequency(0) side effect. This is not a full
 * MainWindow/Qt GUI or real CAT test.
 */
#define WSJT_TRACE_CAT 0
#define Q_EMIT
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

struct FakeImpl
{
  std::vector<std::string> events;
  bool split_mode {true};
  std::int64_t tx_hz {14074000};
  int synchronization_calls {0};
  int zero_frequency_calls {0};
  int clearing_set_effects {0};
  bool last_force_signal {false};

  void sync_transceiver (bool force_signal)
  {
    ++synchronization_calls;
    last_force_signal = force_signal;
    events.emplace_back ("sync");
  }

  void transceiver_tx_frequency (std::int64_t hz)
  {
    if (0 == hz)
      {
        ++zero_frequency_calls;
        ++clearing_set_effects;
        split_mode = false;
        tx_hz = 0;
        events.emplace_back ("tx_frequency_zero");
      }
  }
};

class Configuration
{
public:
  explicit Configuration (FakeImpl * impl = nullptr) : m_ {impl} {}

  FakeImpl * m_ {nullptr};
  bool online {false};
  int online_checks {0};
  int read_only_gets {0};

  bool transceiver_online ()
  {
    ++online_checks;
    m_->events.emplace_back ("online");
    return online;
  }

  void sync_transceiver (bool force_signal, bool enforce_mode_and_split = false);

  bool request_read_only_cat_refresh ()
  {
    ++read_only_gets;
    m_->events.emplace_back ("GET");
    return true;
  }
};

class MainWindow
{
public:
  bool m_transmitting {false};
  Configuration m_config;
  void on_readFreq_clicked ();
};

static bool check (bool condition, char const * description)
{
  if (!condition)
    {
      std::cerr << "FAIL: " << description << '\n';
      return false;
    }
  return true;
}

static bool events_equal (std::vector<std::string> const& actual,
                          std::vector<std::string> const& expected)
{
  return actual == expected;
}

int main ()
{
  int failures {0};
  {
    FakeImpl impl;
    MainWindow window;
    window.m_config = Configuration {&impl};
    window.m_config.online = true;
    window.on_readFreq_clicked ();

    failures += !check (window.m_config.online_checks == 1,
                        "online action checks the existing rig connection once");
    failures += !check (impl.synchronization_calls == 1 && impl.last_force_signal,
                        "online action invokes the production sync wrapper once");
    failures += !check (impl.zero_frequency_calls == 0 && impl.clearing_set_effects == 0,
                        "strong split-preserving sync does not issue the clearing SET path");
    failures += !check (impl.split_mode && impl.tx_hz == 14074000,
                        "online split and existing TX Hz are retained");
    failures += !check (window.m_config.read_only_gets == 1,
                        "online action requests exactly one read-only CAT GET");
    failures += !check (events_equal (impl.events, {"online", "sync", "GET"}),
                        "online check, synchronization, and GET keep their order");
  }
  {
    FakeImpl impl;
    MainWindow window;
    window.m_config = Configuration {&impl};
    window.m_config.online = false;
    window.on_readFreq_clicked ();

    failures += !check (window.m_config.online_checks == 1,
                        "offline action preserves the explicit online check");
    failures += !check (impl.synchronization_calls == 0 && impl.zero_frequency_calls == 0
                        && impl.clearing_set_effects == 0,
                        "offline action does not synchronize or issue a SET");
    failures += !check (window.m_config.read_only_gets == 0,
                        "offline action does not request GET");
    failures += !check (events_equal (impl.events, {"online"}),
                        "offline action stops after the online check");
  }
  {
    FakeImpl impl;
    MainWindow window;
    window.m_config = Configuration {&impl};
    window.m_config.online = true;
    window.m_transmitting = true;
    window.on_readFreq_clicked ();

    failures += !check (window.m_config.online_checks == 0,
                        "transmitting action returns before checking the rig");
    failures += !check (impl.synchronization_calls == 0 && impl.zero_frequency_calls == 0
                        && window.m_config.read_only_gets == 0 && impl.events.empty (),
                        "transmitting action does not connect, synchronize, or GET");
  }
  {
    FakeImpl impl;
    Configuration weak_sync {&impl};
    weak_sync.sync_transceiver (true);

    failures += !check (impl.synchronization_calls == 1 && impl.zero_frequency_calls == 1,
                        "negative control: one-argument sync reaches transceiver_tx_frequency(0)");
    failures += !check (!impl.split_mode && impl.tx_hz == 0 && impl.clearing_set_effects == 1,
                        "negative control: default false mode clears split and TX Hz");
  }

  if (failures)
    {
      std::cerr << "read_frequency_action_test: " << failures << " failure(s)\n";
      return 1;
    }
  std::cout << "PASS read_frequency_action_test (online, offline, transmitting, weak-argument negative control)\n";
  return 0;
}
