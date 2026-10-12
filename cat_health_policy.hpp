#ifndef CAT_HEALTH_POLICY_HPP__
#define CAT_HEALTH_POLICY_HPP__

#include <cstdint>

// Deterministic observer for worker-stamped CAT PTT-read observations.  It
// describes successful Hamlib API reads, not serial-wire or radio proof.
class CatHealthPolicy final
{
public:
  enum class State {unknown, healthy, suspect};
  enum class Reason
  {
    unobserved, awaiting_poll, poll_in_progress, valid_get,
    invalid_get, poll_failed, poll_stalled, poll_slow, get_stale,
    no_valid_get
  };

  struct Snapshot
  {
    State state {State::unknown};
    Reason reason {Reason::unobserved};
    std::int64_t last_valid_get_ns {0};
    std::int64_t last_poll_started_ns {0};
    std::int64_t last_poll_completed_ns {0};
    std::int64_t last_poll_duration_ns {0};
    std::int64_t poll_interval_ms {0};
    std::int64_t last_api_get_result_ns {0};
    std::uint64_t poll_id {0};
    std::uint64_t completed_poll_id {0};
    std::uint64_t ptt_readback_serial {0};
    unsigned control_sequence {0};
    int api_return {0};
    bool protocol_valid {false};
    bool last_api_get_succeeded {false};
    bool last_poll_failed {false};
    bool poll_in_flight {false};
  };

  void begin_session (std::uint64_t generation, bool runtime, bool active,
                      bool supported, std::int64_t worker_time_fence_ns) noexcept
  {
    generation_ = generation;
    eligible_ = runtime && active && supported;
    fence_ns_ = worker_time_fence_ns;
    snapshot_ = Snapshot {};
    have_poll_ = false;
    have_completed_poll_ = false;
    completed_poll_valid_ = false;
    completed_poll_failed_ = false;
    completed_poll_slow_ = false;
    active_read_seen_ = false;
    active_read_valid_ = false;
    have_logged_state_ = false;
    logged_state_ = State::unknown;
    last_log_ns_ = 0;
    snapshot_.reason = eligible_ ? Reason::awaiting_poll : Reason::unobserved;
  }

  void clear () noexcept
  {
    generation_ = 0;
    eligible_ = false;
    fence_ns_ = 0;
    snapshot_ = Snapshot {};
    have_poll_ = false;
    have_completed_poll_ = false;
    completed_poll_valid_ = false;
    completed_poll_failed_ = false;
    completed_poll_slow_ = false;
    active_read_seen_ = false;
    active_read_valid_ = false;
  }

  bool poll_started (std::uint64_t generation, std::uint64_t poll_id,
                     std::int64_t worker_ns,
                     int next_interval_ms) noexcept
  {
    if (!accepts (generation, worker_ns) || !poll_id || poll_id <= snapshot_.poll_id)
      return false;
    have_poll_ = true;
    have_completed_poll_ = false;
    completed_poll_valid_ = false;
    completed_poll_failed_ = false;
    completed_poll_slow_ = false;
    active_read_seen_ = false;
    active_read_valid_ = false;
    snapshot_.poll_id = poll_id;
    snapshot_.last_poll_started_ns = worker_ns;
    snapshot_.poll_interval_ms = next_interval_ms > 0 ? next_interval_ms : 0;
    snapshot_.poll_in_flight = true;
    refresh (worker_ns);
    return true;
  }

  bool poll_read (std::uint64_t generation, std::uint64_t poll_id,
                  std::int64_t worker_ns,
                  bool api_success, bool protocol_valid,
                  unsigned control_sequence, std::uint64_t ptt_readback_serial,
                  int api_return) noexcept
  {
    if (!accepts (generation, worker_ns) || !snapshot_.poll_in_flight
        || poll_id != snapshot_.poll_id)
      return false;
    active_read_seen_ = true;
    active_read_valid_ = api_success && protocol_valid;
    record_get_result (worker_ns, api_success, protocol_valid,
                       control_sequence, ptt_readback_serial, api_return);
    refresh (worker_ns);
    return true;
  }

  bool refresh_read (std::uint64_t generation, std::int64_t worker_ns,
                     bool executed_observation, bool api_success,
                     bool protocol_valid, unsigned control_sequence,
                     std::uint64_t ptt_readback_serial,
                     int api_return) noexcept
  {
    if (!accepts (generation, worker_ns) || !executed_observation) return false;
    record_get_result (worker_ns, api_success, protocol_valid,
                       control_sequence, ptt_readback_serial, api_return);
    refresh (worker_ns);
    return true;
  }

  bool poll_completed (std::uint64_t generation, std::uint64_t poll_id,
                       std::int64_t worker_started_ns,
                       std::int64_t worker_completed_ns,
                       int next_interval_ms, bool poll_failed) noexcept
  {
    if (!accepts (generation, worker_completed_ns) || !snapshot_.poll_in_flight
        || poll_id != snapshot_.poll_id
        || worker_started_ns != snapshot_.last_poll_started_ns
        || worker_completed_ns < worker_started_ns)
      return false;
    snapshot_.last_poll_completed_ns = worker_completed_ns;
    snapshot_.completed_poll_id = poll_id;
    snapshot_.last_poll_duration_ns = worker_completed_ns - worker_started_ns;
    snapshot_.last_poll_failed = poll_failed;
    if (next_interval_ms > 0) snapshot_.poll_interval_ms = next_interval_ms;
    snapshot_.poll_in_flight = false;
    have_completed_poll_ = true;
    completed_poll_valid_ = active_read_valid_;
    completed_poll_failed_ = poll_failed;
    completed_poll_slow_ = snapshot_.last_poll_duration_ns > slow_poll_threshold_ns;
    refresh (worker_completed_ns);
    return true;
  }

  Snapshot evaluate (std::int64_t worker_now_ns) noexcept
  {
    refresh (worker_now_ns);
    return snapshot_;
  }

  bool accepts (std::uint64_t generation, std::int64_t worker_ns) const noexcept
  {
    return eligible_ && generation == generation_ && worker_ns >= fence_ns_;
  }

  // State changes are logged immediately; otherwise at most one summary per
  // 30 seconds.  This clock is used only for log throttling, never freshness.
  bool should_log (std::int64_t monotonic_now_ns) noexcept
  {
    if (!have_logged_state_ || snapshot_.state != logged_state_
        || monotonic_now_ns - last_log_ns_ >= 30000000000LL)
      {
        have_logged_state_ = true;
        logged_state_ = snapshot_.state;
        last_log_ns_ = monotonic_now_ns;
        return true;
      }
    return false;
  }

  static std::int64_t freshness_window_ns (std::int64_t interval_ms) noexcept
  {
    auto const interval_ns = interval_ms > 0 ? interval_ms * 1000000LL : 0LL;
    return interval_ns + 3000000000LL;
  }

  static constexpr std::int64_t slow_poll_threshold_ns = 5000000000LL;

private:
  void record_get_result (std::int64_t worker_ns, bool api_success,
                          bool protocol_valid, unsigned control_sequence,
                          std::uint64_t ptt_readback_serial,
                          int api_return) noexcept
  {
    auto const valid = api_success && protocol_valid;
    snapshot_.last_api_get_result_ns = worker_ns;
    snapshot_.last_api_get_succeeded = valid;
    snapshot_.api_return = api_return;
    snapshot_.protocol_valid = protocol_valid;
    snapshot_.control_sequence = control_sequence;
    snapshot_.ptt_readback_serial = ptt_readback_serial;
    if (valid) snapshot_.last_valid_get_ns = worker_ns;
  }

  void refresh (std::int64_t now_ns) noexcept
  {
    if (!eligible_ || !have_poll_)
      {
        snapshot_.state = State::unknown;
        snapshot_.reason = Reason::unobserved;
        return;
      }

    if (snapshot_.poll_in_flight)
      {
        auto const age = now_ns >= snapshot_.last_poll_started_ns
          ? now_ns - snapshot_.last_poll_started_ns : 0;
        if (age > slow_poll_threshold_ns)
          {
            snapshot_.state = State::suspect;
            snapshot_.reason = Reason::poll_stalled;
          }
        else if ((active_read_seen_ && !active_read_valid_)
                 || (snapshot_.last_api_get_result_ns
                     >= snapshot_.last_poll_started_ns
                     && !snapshot_.last_api_get_succeeded))
          {
            snapshot_.state = State::suspect;
            snapshot_.reason = Reason::invalid_get;
          }
        else if (snapshot_.last_valid_get_ns
                 && now_ns - snapshot_.last_valid_get_ns
                    <= freshness_window_ns (snapshot_.poll_interval_ms))
          {
            snapshot_.state = State::healthy;
            snapshot_.reason = Reason::valid_get;
          }
        else
          {
            snapshot_.state = State::unknown;
            snapshot_.reason = Reason::poll_in_progress;
          }
        return;
      }

    if (!have_completed_poll_)
      {
        snapshot_.state = State::unknown;
        snapshot_.reason = Reason::awaiting_poll;
        return;
      }

    if (completed_poll_failed_
        && snapshot_.last_valid_get_ns <= snapshot_.last_poll_completed_ns)
      {
        snapshot_.state = State::suspect;
        snapshot_.reason = Reason::poll_failed;
        return;
      }

    if (completed_poll_slow_
        && snapshot_.last_valid_get_ns <= snapshot_.last_poll_completed_ns)
      {
        snapshot_.state = State::suspect;
        snapshot_.reason = Reason::poll_slow;
        return;
      }

    if (!completed_poll_valid_
        && snapshot_.last_valid_get_ns <= snapshot_.last_poll_completed_ns)
      {
        snapshot_.state = State::suspect;
        snapshot_.reason = active_read_seen_ ? Reason::invalid_get
                                             : Reason::no_valid_get;
        return;
      }

    if (snapshot_.last_api_get_result_ns >= snapshot_.last_valid_get_ns
        && !snapshot_.last_api_get_succeeded)
      {
        snapshot_.state = State::suspect;
        snapshot_.reason = Reason::invalid_get;
        return;
      }

    auto const age = now_ns >= snapshot_.last_valid_get_ns
      ? now_ns - snapshot_.last_valid_get_ns : 0;
    if (age <= freshness_window_ns (snapshot_.poll_interval_ms))
      {
        snapshot_.state = State::healthy;
        snapshot_.reason = Reason::valid_get;
      }
    else
      {
        snapshot_.state = State::suspect;
        snapshot_.reason = Reason::get_stale;
      }
  }

  std::uint64_t generation_ {0};
  bool eligible_ {false};
  std::int64_t fence_ns_ {0};
  Snapshot snapshot_;
  bool have_poll_ {false};
  bool have_completed_poll_ {false};
  bool completed_poll_valid_ {false};
  bool completed_poll_failed_ {false};
  bool completed_poll_slow_ {false};
  bool active_read_seen_ {false};
  bool active_read_valid_ {false};
  bool have_logged_state_ {false};
  State logged_state_ {State::unknown};
  std::int64_t last_log_ns_ {0};
};

#endif
