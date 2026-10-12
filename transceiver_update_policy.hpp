#ifndef TRANSCEIVER_UPDATE_POLICY_HPP__
#define TRANSCEIVER_UPDATE_POLICY_HPP__

#include <cstdint>

// Classifies one Transceiver::update before Configuration mutates its request
// cache or forwards state to MainWindow.  Generation establishes object
// ownership; command sequence establishes whether an online snapshot may
// replace the current accepted state.  An offline edge from a generation that
// was confirmed online is terminal even when its command sequence is old.
namespace TransceiverUpdatePolicy
{
  enum class Action
  {
    ignore_stale_generation,
    ignore_unconfirmed_offline,
    ignore_stale_sequence,
    accept_current_online,
    report_confirmed_offline
  };

  inline Action classify (std::uint64_t update_generation,
                          std::uint64_t current_generation,
                          unsigned update_sequence,
                          unsigned current_sequence,
                          bool online,
                          bool generation_confirmed_online) noexcept
  {
    if (update_generation != current_generation)
      return Action::ignore_stale_generation;

    if (!online)
      return generation_confirmed_online
        ? Action::report_confirmed_offline
        : Action::ignore_unconfirmed_offline;

    return update_sequence == current_sequence
      ? Action::accept_current_online
      : Action::ignore_stale_sequence;
  }
}

#endif
