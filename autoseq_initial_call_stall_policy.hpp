#ifndef AUTOSEQ_INITIAL_CALL_STALL_POLICY_HPP__
#define AUTOSEQ_INITIAL_CALL_STALL_POLICY_HPP__

#include <cstdint>
#include <string>
#include <utility>

// Owns only an unattended AutoSeq CQ/RFIN initial target. The caller supplies
// steady-clock milliseconds and performs all UI, CAT, history, and TX actions.
class AutoSeqInitialCallStallPolicy
{
public:
  static constexpr std::int64_t timeout_ms = 120000;
  static constexpr std::int64_t snapshot_interval_ms = 60000;

  enum class Action { none, snapshot, blocked, release };

  struct Decision
  {
    Decision () = default;
    Decision (Action value, std::string reason_value, std::uint64_t suppressed_value)
      : action {value}, reason {std::move (reason_value)}, suppressed {suppressed_value}
    {
    }

    Action action {Action::none};
    std::string reason;
    std::uint64_t suppressed {0};
  };

  void select (std::string call, std::string base_call, int report,
               std::string band, std::string mode, std::uint64_t attempt_id,
               std::int64_t monotonic_ms)
  {
    call_ = std::move (call);
    base_call_ = std::move (base_call);
    band_ = std::move (band);
    mode_ = std::move (mode);
    report_ = report;
    attempt_id_ = attempt_id;
    selected_at_ms_ = monotonic_ms;
    last_progress_at_ms_ = monotonic_ms;
    last_snapshot_at_ms_ = monotonic_ms;
    active_ = !call_.empty () && !base_call_.empty ();
    blocked_reason_.clear ();
    suppressed_block_count_ = 0;
    last_progress_.assign ("selection");
  }

  bool active () const {return active_;}
  std::string const& call () const {return call_;}
  std::string const& base_call () const {return base_call_;}
  std::string const& band () const {return band_;}
  std::string const& mode () const {return mode_;}
  std::string const& last_progress () const {return last_progress_;}
  int report () const {return report_;}
  std::uint64_t attempt_id () const {return attempt_id_;}
  std::int64_t selected_at_ms () const {return selected_at_ms_;}
  std::int64_t last_progress_at_ms () const {return last_progress_at_ms_;}

  std::int64_t elapsed_ms (std::int64_t now_ms) const
  {
    return active_ && now_ms >= last_progress_at_ms_
      ? now_ms - last_progress_at_ms_ : 0;
  }

  void update_report (int report)
  {
    if (active_) report_ = report;
  }

  // Only actual PTT/audio confirmation is passed here. Third-party decodes,
  // CQ/RFIN repeats, Enable Tx, and message-slot selection do not refresh it.
  void observe_actual_progress (std::int64_t monotonic_ms, char const * kind)
  {
    if (!active_) return;
    if (monotonic_ms >= last_progress_at_ms_)
      last_progress_at_ms_ = monotonic_ms;
    last_progress_.assign (kind ? kind : "actual_tx");
    blocked_reason_.clear ();
    suppressed_block_count_ = 0;
  }

  bool is_owned_call (std::string const& call, std::string const& base_call) const
  {
    return active_ && (call == call_ || (!base_call.empty () && base_call == base_call_));
  }

  // The caller sets this only after seeing a targeted message in the current
  // live decode batch and confirming the latest history status is a valid
  // directed QSO phase.
  bool observe_valid_directed_reply (std::string const& call,
                                     std::string const& base_call)
  {
    if (!is_owned_call (call, base_call)) return false;
    cancel ();
    return true;
  }

  void cancel ()
  {
    active_ = false;
    call_.clear ();
    base_call_.clear ();
    band_.clear ();
    mode_.clear ();
    report_ = -60;
    attempt_id_ = 0;
    selected_at_ms_ = 0;
    last_progress_at_ms_ = 0;
    last_snapshot_at_ms_ = 0;
    last_progress_.clear ();
    blocked_reason_.clear ();
    suppressed_block_count_ = 0;
  }

  // A release is considered only for a complete, fresh, live batch. All
  // hardware/control safety facts must be supplied by the MainWindow adapter.
  Decision evaluate (std::int64_t now_ms, bool complete_fresh_live_batch,
                     bool auto_sequence_enabled, bool current_target_matches,
                     bool history_initial_stage, bool cat_online,
                     bool ptt_readback_known, bool ptt_off,
                     bool ptt_readback_after_attempt,
                     bool transmitting, bool tune, bool ptt_on_or_pending,
                     bool recovery_pending, bool manual_or_disk_decode)
  {
    if (!active_ || !complete_fresh_live_batch) return {};
    if (!auto_sequence_enabled || !current_target_matches || !history_initial_stage
        || manual_or_disk_decode)
      {
        cancel ();
        return {Action::none, "ownership_ended", 0};
      }

    if (elapsed_ms (now_ms) < timeout_ms)
      return make_snapshot_if_due (now_ms);

    std::string reason;
    if (!cat_online) reason = "cat_offline";
    else if (!ptt_readback_known) reason = "ptt_unknown";
    else if (!ptt_readback_after_attempt) reason = "ptt_readback_before_attempt";
    else if (!ptt_off) reason = "ptt_on";
    else if (transmitting) reason = "transmitting";
    else if (tune) reason = "tune";
    else if (ptt_on_or_pending) reason = "ptt_or_tx_pending";
    else if (recovery_pending) reason = "recovery_pending";

    if (!reason.empty ()) return note_blocked (std::move (reason));

    auto const suppressed = suppressed_block_count_;
    active_ = false;
    return {Action::release, std::string {}, suppressed};
  }

private:
  Decision make_snapshot_if_due (std::int64_t now_ms)
  {
    if (now_ms < last_snapshot_at_ms_
        || now_ms - last_snapshot_at_ms_ < snapshot_interval_ms)
      return {};
    last_snapshot_at_ms_ = now_ms;
    auto const suppressed = suppressed_block_count_;
    suppressed_block_count_ = 0;
    return {Action::snapshot, blocked_reason_, suppressed};
  }

  Decision note_blocked (std::string reason)
  {
    if (blocked_reason_ == reason)
      {
        ++suppressed_block_count_;
        return {};
      }
    auto const suppressed = suppressed_block_count_;
    blocked_reason_ = std::move (reason);
    suppressed_block_count_ = 0;
    return {Action::blocked, blocked_reason_, suppressed};
  }

  bool active_ {false};
  int report_ {-60};
  std::uint64_t attempt_id_ {0};
  std::int64_t selected_at_ms_ {0};
  std::int64_t last_progress_at_ms_ {0};
  std::int64_t last_snapshot_at_ms_ {0};
  std::uint64_t suppressed_block_count_ {0};
  std::string call_;
  std::string base_call_;
  std::string band_;
  std::string mode_;
  std::string last_progress_;
  std::string blocked_reason_;
};

#endif
