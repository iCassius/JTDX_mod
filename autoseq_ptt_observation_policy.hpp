#ifndef AUTOSEQ_PTT_OBSERVATION_POLICY_HPP__
#define AUTOSEQ_PTT_OBSERVATION_POLICY_HPP__

#include <cstdint>

// Adapts a current rig-state callback into the freshness evidence used by
// AutoSeq's initial-call release gate. Legacy state updates are observations,
// not claims that a hardware GET occurred. Strict backends require a newer
// successful-read serial before an observation can become fresh.
class AutoSeqPttObservationPolicy
{
public:
  struct Snapshot
  {
    bool known {false};
    bool on {false};
    std::int64_t observed_at_ms {0};
    std::uint64_t serial {0};

    Snapshot () = default;
    Snapshot (bool known_, bool on_, std::int64_t observed_at_ms_,
              std::uint64_t serial_)
      : known {known_}
      , on {on_}
      , observed_at_ms {observed_at_ms_}
      , serial {serial_}
    {
    }
  };

  enum class Action
  {
    ignored_old_generation,
    invalidated_offline,
    invalidated_unknown,
    ignored_stale_serial,
    accepted_legacy_state,
    accepted_real_get
  };

  struct Decision
  {
    Snapshot snapshot;
    Action action {Action::ignored_old_generation};

    Decision () = default;
    Decision (Snapshot snapshot_, Action action_)
      : snapshot {snapshot_}
      , action {action_}
    {
    }

    bool accepted () const
    {
      return action == Action::accepted_legacy_state
        || action == Action::accepted_real_get;
    }

    bool real_get_advanced () const
    {
      return action == Action::accepted_real_get;
    }
  };

  // A new PTT request invalidates prior safety evidence while retaining the
  // serial floor, so a delayed strict-backend read cannot satisfy the request.
  static Snapshot invalidate_for_request (Snapshot snapshot)
  {
    snapshot.known = false;
    snapshot.on = false;
    snapshot.observed_at_ms = 0;
    return snapshot;
  }

  static Decision observe (Snapshot previous, bool current_generation,
                           bool online, bool readback_required,
                           bool readback_known, bool ptt_on,
                           std::uint64_t readback_serial,
                           std::int64_t now_ms)
  {
    if (!current_generation)
      return {previous, Action::ignored_old_generation};

    if (!online)
      {
        previous.known = false;
        previous.on = false;
        previous.observed_at_ms = 0;
        return {previous, Action::invalidated_offline};
      }

    if (readback_required)
      {
        if (!readback_known)
          {
            previous.known = false;
            previous.on = false;
            previous.observed_at_ms = 0;
            return {previous, Action::invalidated_unknown};
          }
        if (readback_serial <= previous.serial)
          return {previous, Action::ignored_stale_serial};

        return {Snapshot {true, ptt_on, now_ms, readback_serial},
                Action::accepted_real_get};
      }

    // A legacy online callback may refresh the observed-state timestamp even
    // when PTT is unchanged. The caller separately decides whether an actual
    // off-to-on edge counts as transmit progress.
    return {Snapshot {true, ptt_on, now_ms, previous.serial},
            Action::accepted_legacy_state};
  }
};

#endif
