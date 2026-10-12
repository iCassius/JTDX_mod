#include "autoseq_initial_call_stall_policy.hpp"
#include "auto_call_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  AutoSeqInitialCallStallPolicy selected (std::int64_t now = 1000)
  {
    AutoSeqInitialCallStallPolicy policy;
    policy.select ("K1ABC", "K1ABC", -12, "20m", "FT8", 9, now);
    return policy;
  }

  AutoSeqInitialCallStallPolicy::Decision evaluate_safe (
      AutoSeqInitialCallStallPolicy& policy, std::int64_t now,
      bool fresh_batch = true)
  {
    return policy.evaluate (now, fresh_batch, true, true, true,
                            true, true, true, true,
                            false, false, false, false, false);
  }

  AutoSeqInitialCallStallPolicy::Decision evaluate_stage (
      AutoSeqInitialCallStallPolicy& policy, std::int64_t now,
      bool initial_stage)
  {
    return policy.evaluate (now, true, true, true, initial_stage,
                            true, true, true, true,
                            false, false, false, false, false);
  }
}

int main ()
{
  using Policy = AutoSeqInitialCallStallPolicy;
  check (AutoCallPolicy::receivedReportOrWeakest (QString {}) == -60,
         "empty received reports retain the weakest-value cooldown behavior");
  check (AutoCallPolicy::receivedReportOrWeakest (QStringLiteral ("bad")) == -60,
         "invalid received reports cannot become zero-strength cooldowns");

  auto boundary = selected (1000);
  auto const before_timeout = evaluate_safe (boundary, 120999, true);
  check (before_timeout.action != Policy::Action::release && boundary.active (),
         "119999 ms does not release the initial target");
  check (evaluate_safe (boundary, 121000, false).action == Policy::Action::none,
         "an incomplete or stale batch cannot release the target");
  check (evaluate_safe (boundary, 121000).action == Policy::Action::release,
         "120000 ms boundary releases only on a fresh live batch");

  auto refreshed = selected (1000);
  check (evaluate_safe (refreshed, 61000).action == Policy::Action::snapshot,
         "active ownership emits a bounded 60-second snapshot");
  refreshed.observe_actual_progress (80000, "ptt_confirmed");
  check (refreshed.elapsed_ms (199999) == 119999,
         "actual TX progress refreshes the monotonic no-progress clock");
  auto const before_refreshed_timeout = evaluate_safe (refreshed, 199999);
  check (before_refreshed_timeout.action != Policy::Action::release && refreshed.active (),
         "actual TX progress prevents an early release");
  check (evaluate_safe (refreshed, 200000).action == Policy::Action::release,
         "release occurs at 120 seconds after the latest actual progress");

  auto third_party = selected (1000);
  // The policy exposes no generic decode-progress method. Unrelated decodes
  // therefore cannot refresh the timer.
  check (evaluate_safe (third_party, 121000).action == Policy::Action::release,
         "third-party traffic does not count as progress");

  auto late_reply = selected (1000);
  check (late_reply.is_owned_call ("K1ABC/P", "K1ABC"),
         "base-call matching accepts a valid directed reply from the owner");
  check (late_reply.observe_valid_directed_reply ("K1ABC/P", "K1ABC"),
         "same-batch valid directed reply disarms initial-stall ownership");
  check (evaluate_safe (late_reply, 121000).action == Policy::Action::none,
         "late QSO continuation wins over timeout");

  auto normal_report_stage = selected (1000);
  check (evaluate_stage (normal_report_stage, 121000, false).action == Policy::Action::none
             && !normal_report_stage.active (),
         "ordinary REPORT history cannot be treated as a stalled initial call");
  auto skip_tx1_stage = selected (1000);
  check (evaluate_stage (skip_tx1_stage, 121000, true).action == Policy::Action::release,
         "the adapter may release a verified skip-Tx1 SREPORT initial stage");

  auto stopped = selected (1000);
  stopped.cancel ();
  check (!stopped.active ()
             && evaluate_safe (stopped, 500000).action == Policy::Action::none,
         "operator cancellation cannot release or re-arm an owner");

  struct Unsafe
  {
    Unsafe (char const * reason_value, bool cat_online_value = true,
            bool ptt_known_value = true, bool ptt_off_value = true,
            bool readback_after_attempt_value = true,
            bool transmitting_value = false, bool tune_value = false,
            bool ptt_pending_value = false, bool recovery_pending_value = false)
      : reason {reason_value}, cat_online {cat_online_value},
        ptt_known {ptt_known_value}, ptt_off {ptt_off_value},
        readback_after_attempt {readback_after_attempt_value},
        transmitting {transmitting_value}, tune {tune_value},
        ptt_pending {ptt_pending_value}, recovery_pending {recovery_pending_value}
    {
    }

    char const * reason;
    bool cat_online;
    bool ptt_known;
    bool ptt_off;
    bool readback_after_attempt;
    bool transmitting;
    bool tune;
    bool ptt_pending;
    bool recovery_pending;
  };
  Unsafe unsafe[] {
    {"cat_offline", false},
    {"ptt_unknown", true, false},
    {"ptt_readback_before_attempt", true, true, true, false},
    {"ptt_on", true, true, false},
    {"transmitting", true, true, true, true, true},
    {"tune", true, true, true, true, false, true},
    {"ptt_or_tx_pending", true, true, true, true, false, false, true},
    {"recovery_pending", true, true, true, true, false, false, false, true},
  };
  for (auto const& gate : unsafe)
    {
      auto guarded = selected (1000);
      auto decision = guarded.evaluate (
        121000, true, true, true, true, gate.cat_online, gate.ptt_known,
        gate.ptt_off, gate.readback_after_attempt,
        gate.transmitting, gate.tune, gate.ptt_pending, gate.recovery_pending, false);
      check (decision.action == Policy::Action::blocked
                 && decision.reason == gate.reason,
             "each CAT/PTT/TX/recovery uncertainty fails closed");
    }

  auto dedup = selected (1000);
  auto first_block = dedup.evaluate (121000, true, true, true, true,
                                     false, true, true, true,
                                     false, false, false, false, false);
  auto second_block = dedup.evaluate (122000, true, true, true, true,
                                      false, true, true, true,
                                      false, false, false, false, false);
  check (first_block.action == Policy::Action::blocked
             && second_block.action == Policy::Action::none,
         "the same blocked reason is logged once while repeated batches are suppressed");
  auto changed_block = dedup.evaluate (123000, true, true, true, true,
                                       true, false, true, true,
                                       false, false, false, false, false);
  check (changed_block.action == Policy::Action::blocked
             && changed_block.reason == "ptt_unknown"
             && changed_block.suppressed == 1,
         "a changed blocked reason records the suppressed count");
  auto released = selected (1000);
  auto blocked = released.evaluate (121000, true, true, true, true,
                                    false, true, true, true,
                                    false, false, false, false, false);
  (void) blocked;
  auto allowed_after_guard = evaluate_safe (released, 122000);
  check (allowed_after_guard.action == Policy::Action::release
             && allowed_after_guard.suppressed == 0,
         "a later fresh batch releases after all safety gates become known-safe");

  auto changed_owner = selected (1000);
  changed_owner.select ("K2XYZ", "K2XYZ", -8, "40m", "FT8", 10, 5000);
  check (changed_owner.call () == "K2XYZ" && changed_owner.attempt_id () == 10
             && changed_owner.elapsed_ms (5000) == 0,
         "ownership follows the actual replacement target and resets its clock");
  return 0;
}
