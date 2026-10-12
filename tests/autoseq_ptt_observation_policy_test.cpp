#include "autoseq_initial_call_stall_policy.hpp"
#include "autoseq_ptt_observation_policy.hpp"

#include <cstdlib>
#include <iostream>

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

  using Observation = AutoSeqPttObservationPolicy;
  using Stall = AutoSeqInitialCallStallPolicy;

  Stall selected (std::int64_t when = 1000)
  {
    Stall policy;
    policy.select ("K1ABC", "K1ABC", -12, "20m", "FT8", 9, when);
    return policy;
  }

  Stall::Decision evaluate (Stall& policy, Observation::Snapshot const& ptt,
                            std::int64_t now, bool cat_online = true,
                            bool fresh_batch = true, bool pending = false,
                            bool recovery = false, bool initial_stage = true,
                            bool manual = false)
  {
    bool const after_attempt = ptt.known
      && ptt.observed_at_ms >= policy.selected_at_ms ();
    return policy.evaluate (now, fresh_batch, true, true, initial_stage,
                            cat_online, ptt.known,
                            ptt.known && !ptt.on, after_attempt,
                            false, false, pending, recovery, manual);
  }
}

int main ()
{
  using Policy = AutoSeqPttObservationPolicy;
  using Action = Policy::Action;

  auto stall = selected (1000);
  auto ptt = Policy::observe ({}, true, true, false, false, false, 0, 900).snapshot;
  check (ptt.known && !ptt.on && ptt.observed_at_ms == 900,
         "legacy online/off state is an observation, not a hardware-read claim");
  ptt = Policy::invalidate_for_request (ptt);
  check (!ptt.known && ptt.observed_at_ms == 0,
         "a new request invalidates a previously safe legacy state");
  auto blocked = evaluate (stall, ptt, 121000);
  check (blocked.action == Stall::Action::blocked && blocked.reason == "ptt_unknown",
         "pre-request state cannot satisfy the post-request release gate");

  auto const legacyOff = Policy::observe (
    ptt, true, true, false, false, false, 0, 120000);
  ptt = legacyOff.snapshot;
  check (legacyOff.action == Action::accepted_legacy_state && ptt.known
             && !ptt.on && ptt.observed_at_ms == 120000,
         "a same-state legacy online/off callback refreshes observed-state time without an edge");
  auto const released = evaluate (stall, ptt, 121000);
  check (released.action == Stall::Action::release,
         "fresh legacy PTT-off observation plus fresh batch releases after 120 seconds");

  auto strictStall = selected (1000);
  auto strict = Policy::observe ({}, true, true, true, true, false, 10, 900).snapshot;
  strict = Policy::invalidate_for_request (strict);
  auto const staleSerial = Policy::observe (
    strict, true, true, true, true, false, 10, 120000);
  check (staleSerial.action == Action::ignored_stale_serial
             && !staleSerial.snapshot.known && staleSerial.snapshot.observed_at_ms == 0,
         "a strict-backend serial at the request baseline remains stale");
  check (evaluate (strictStall, staleSerial.snapshot, 121000).reason == "ptt_unknown",
         "stale strict GET cannot open the release gate");
  auto const newGet = Policy::observe (
    staleSerial.snapshot, true, true, true, true, false, 11, 120000);
  check (newGet.action == Action::accepted_real_get
             && newGet.snapshot.known && !newGet.snapshot.on
             && newGet.snapshot.observed_at_ms == 120000
             && newGet.snapshot.serial == 11,
         "only a known, advanced real-GET serial refreshes strict evidence");
  check (evaluate (strictStall, newGet.snapshot, 121000).action == Stall::Action::release,
         "advanced strict GET plus the other safe gates permits release");

  auto serialFloorBeforeGenerationReset = Policy::observe (
    {}, true, true, true, true, false, 100, 1000).snapshot;
  auto const resetForNewGeneration = Policy::Snapshot {};
  auto const firstReadOfNewGeneration = Policy::observe (
    resetForNewGeneration, true, true, true, true, false, 1, 2000);
  check (serialFloorBeforeGenerationReset.serial == 100
             && firstReadOfNewGeneration.action == Action::accepted_real_get
             && firstReadOfNewGeneration.snapshot.serial == 1,
         "generation reset discards the old serial floor so a new session can accept GET serial 1");

  auto const unknown = Policy::observe (
    newGet.snapshot, true, true, true, false, false, 12, 122000);
  check (unknown.action == Action::invalidated_unknown && !unknown.snapshot.known
             && unknown.snapshot.observed_at_ms == 0,
         "unknown strict readback invalidates PTT-off evidence");
  auto const offline = Policy::observe (
    ptt, true, false, false, false, false, 0, 123000);
  check (offline.action == Action::invalidated_offline && !offline.snapshot.known,
         "offline state cannot be treated as safe PTT-off evidence");

  auto beforeGeneration = Policy::invalidate_for_request (
    Policy::observe ({}, true, true, false, false, false, 0, 900).snapshot);
  auto const oldGeneration = Policy::observe (
    beforeGeneration, false, true, false, false, false, 0, 120000);
  check (oldGeneration.action == Action::ignored_old_generation
             && !oldGeneration.snapshot.known && oldGeneration.snapshot.observed_at_ms == 0,
         "old-generation callback cannot refresh invalidated evidence");

  auto unsafeStall = selected (1000);
  auto knownOn = Policy::observe ({}, true, true, false, false, true, 0, 120000).snapshot;
  auto const pttOn = evaluate (unsafeStall, knownOn, 121000);
  check (pttOn.action == Stall::Action::blocked && pttOn.reason == "ptt_on",
         "an online PTT-on observation cannot release the owner");
  auto knownOff = Policy::observe ({}, true, true, false, false, false, 0, 120000).snapshot;
  check (evaluate (unsafeStall, knownOff, 121000, true, true, true).reason
             == "ptt_or_tx_pending",
         "pending PTT/TX remains a release blocker");
  check (evaluate (unsafeStall, knownOff, 121000, true, true, false, true).reason
             == "recovery_pending",
         "a recovery ticket remains protected");
  auto manualStall = selected (1000);
  auto const manual = evaluate (manualStall, knownOff, 121000, true, true,
                                false, false, true, true);
  check (manual.action == Stall::Action::none && !manualStall.active (),
         "manual decode ends owner handling without an automatic release");
  auto qsoStall = selected (1000);
  auto const qso = evaluate (qsoStall, knownOff, 121000, true, true,
                             false, false, false);
  check (qso.action == Stall::Action::none && !qsoStall.active (),
         "a non-initial QSO history stage is never cleared by stall release");
  auto offlineStall = selected (1000);
  check (evaluate (offlineStall, offline.snapshot, 121000, false).reason == "cat_offline",
         "offline CAT remains a release blocker even after a fresh batch");

  auto progressStall = selected (1000);
  auto progressPtt = Policy::observe ({}, true, true, false, false, false, 0, 3000).snapshot;
  auto const firstOn = Policy::observe (
    progressPtt, true, true, false, false, true, 0, 10000);
  progressPtt = firstOn.snapshot;
  // MainWindow's existing token-qualified off-to-on edge is the only call to
  // observe_actual_progress. Same-state callbacks below only refresh the
  // state-observation timestamp in the helper.
  progressStall.observe_actual_progress (10000, "legacy_state");
  auto const repeatedOn = Policy::observe (
    progressPtt, true, true, false, false, true, 0, 50000);
  progressPtt = repeatedOn.snapshot;
  auto const laterOff = Policy::observe (
    progressPtt, true, true, false, false, false, 0, 120000);
  progressPtt = laterOff.snapshot;
  check (progressPtt.observed_at_ms == 120000
             && progressStall.last_progress_at_ms () == 10000
             && progressStall.elapsed_ms (130000) == 120000,
         "repeated state observation never refreshes the actual-progress clock");
  check (evaluate (progressStall, progressPtt, 130000).action == Stall::Action::release,
         "state-observation freshness and the 120-second no-progress clock remain independent");
  return 0;
}
