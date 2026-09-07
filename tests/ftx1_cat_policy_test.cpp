#include "ftx1_cat_poll_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * message)
  {
    if (!condition)
      {
        std::cerr << message << '\n';
        std::exit (1);
      }
  }

  Ftx1CatPollPolicy::Context idle (bool enable_tx = false)
  {
    Ftx1CatPollPolicy::Context context;
    context.ftx1 = true;
    context.read_only = true;
    context.transient_error = true;
    context.ptt_known = true;
    context.enable_tx = enable_tx;
    return context;
  }
}

int main ()
{
  using namespace Ftx1CatPollPolicy;

  // a, b: FTX-1 standby soft ignores one/two failures, then escalates.
  State policy;
  auto const standby = idle ();
  require (Decision::soft_ignore == policy.observe_failure (Operation::rx_frequency, standby),
           "FTX-1 standby first EPROTO must be soft ignored");
  require (1 == policy.failure_count (Operation::rx_frequency),
           "first failure must be counted");
  require (Decision::soft_ignore == policy.observe_failure (Operation::rx_frequency, standby),
           "FTX-1 standby second transient failure must be soft ignored");
  require (Decision::escalate == policy.observe_failure (Operation::rx_frequency, standby),
           "third consecutive failure must escalate");

  // c: a successful read clears only the affected operation's streak.
  require (3 == policy.observe_success (Operation::rx_frequency),
           "success must report and clear the failure streak");
  require (0 == policy.failure_count (Operation::rx_frequency),
           "successful read must clear the failure streak");

  // d, e: non-FTX-1 and write failures retain hard-failure behavior.
  auto non_ftx1 = standby;
  non_ftx1.ftx1 = false;
  require (Decision::hard_failure == policy.observe_failure (Operation::mode, non_ftx1),
           "non-FTX-1 behavior must remain hard");
  auto write_failure = standby;
  write_failure.read_only = false;
  require (Decision::hard_failure == policy.observe_failure (Operation::ptt, write_failure),
           "write command failure must remain hard");

  // f: PTT request/actual state uncertainty is never soft ignored.
  auto pending_ptt = standby;
  pending_ptt.ptt_request_pending = true;
  require (Decision::hard_failure == policy.observe_failure (Operation::ptt, pending_ptt),
           "pending PTT request must remain hard");
  auto transitioning_ptt = standby;
  transitioning_ptt.ptt_transition_pending = true;
  require (Decision::hard_failure == policy.observe_failure (Operation::ptt, transitioning_ptt),
           "PTT transition hold must remain hard");
  auto transmitting = standby;
  transmitting.ptt_actual = true;
  require (Decision::hard_failure == policy.observe_failure (Operation::rx_frequency, transmitting),
           "actual PTT on must remain hard");
  auto unknown_ptt = standby;
  unknown_ptt.ptt_known = false;
  require (Decision::hard_failure == policy.observe_failure (Operation::rx_frequency, unknown_ptt),
           "unknown PTT state must remain hard");

  // g: Enable Tx alone does not veto a standby read-only soft ignore.
  require (Decision::soft_ignore == policy.observe_failure (Operation::vfo, idle (true)),
           "Enable Tx armed with actual standby must allow soft ignore");
  require (!nonessential_reads_allowed (true, true, true, false, false, 0, true),
           "PTT intent must pause nonessential reads");
  require (nonessential_reads_allowed (true, true, false, false, false, 0, true),
           "Enable Tx alone must not pause safe standby reads");
  require (!meter_reads_allowed (true, true, false, false, false, 0, 0),
           "FTX-1 standby must not use the TX-only meter path");
  require (!meter_reads_allowed (true, false, true, true, false, 0, 0),
           "unknown PTT state must block the meter path");
  require (!meter_reads_allowed (true, true, true, true, true, 0, 0),
           "pending PTT request must block the meter path");
  require (!meter_reads_allowed (true, true, true, true, false, 1, 0),
           "non-essential transition hold must block the meter path");
  require (!meter_reads_allowed (true, true, true, true, false, 0, 1),
           "PTT transition hold must block the meter path");
  require (meter_reads_allowed (true, true, true, true, false, 0, 0),
           "confirmed FTX-1 TX after both holds must allow meters");
  require (meter_reads_allowed (false, false, false, false, true, 2, 2),
           "non-FTX-1 meter policy must remain unrestricted");
  require (legacy_optional_failure (Operation::mode, true, idle ()),
           "FTX-1 optional mode ENAVAIL/ENIMPL/EINVAL must retain old ignore semantics");
  require (!legacy_optional_failure (Operation::rx_frequency, true, idle ()),
           "frequency read is not an optional legacy-ignore operation");
  auto optional_pending = idle ();
  optional_pending.ptt_request_pending = true;
  require (!legacy_optional_failure (Operation::mode, true, optional_pending),
           "optional query must not ignore errors during a PTT transition");
  auto optional_transmitting = idle ();
  optional_transmitting.ptt_intent = true;
  optional_transmitting.ptt_actual = true;
  require (!legacy_optional_failure (Operation::power, true, optional_transmitting),
           "meter errors during TX must retain hard-failure semantics");
  require (nonessential_reads_allowed (false, false, true, true, true, 2, true),
           "non-FTX-1 polling schedule must remain unchanged");

  // A legacy optional result completes the poll and clears the overall
  // streak; a later transient error starts again at the first failure.
  State optional_completion;
  optional_completion.begin_poll ();
  require (Decision::soft_ignore == optional_completion.observe_failure (
      Operation::rx_frequency, idle ()),
           "transient failure before optional completion must be counted");
  require (1 == optional_completion.overall_failure_streak (),
           "first transient failure must set overall=1");
  optional_completion.begin_poll ();
  require (legacy_optional_failure (Operation::mode, true, idle ()),
           "optional legacy error must complete safely");
  optional_completion.complete_poll ();
  require (0 == optional_completion.overall_failure_streak (),
           "optional legacy completion must clear overall streak");
  optional_completion.begin_poll ();
  require (Decision::soft_ignore == optional_completion.observe_failure (
      Operation::rx_frequency, idle ()),
           "transient failure after optional completion must be soft again");
  require (1 == optional_completion.overall_failure_streak (),
           "transient failure after completion must restart at overall=1");

  // Alternating read operations still reach the bounded overall threshold;
  // per-operation counters alone must not allow an endless failure sequence.
  State aggregate;
  auto aggregate_failure = idle ();
  aggregate.begin_poll ();
  require (Decision::soft_ignore == aggregate.observe_failure (Operation::vfo, aggregate_failure),
           "first poll failure must be soft ignored");
  aggregate.begin_poll ();
  require (Decision::soft_ignore == aggregate.observe_failure (Operation::mode, aggregate_failure),
           "second alternating poll failure must be soft ignored");
  aggregate.begin_poll ();
  require (Decision::escalate == aggregate.observe_failure (Operation::strength, aggregate_failure),
           "third alternating poll failure must escalate overall");
  aggregate.reset ();
  aggregate.begin_poll ();
  aggregate.complete_poll ();
  require (0 == aggregate.overall_failure_streak (),
           "a fully successful poll must clear the overall failure streak");

  return 0;
}
