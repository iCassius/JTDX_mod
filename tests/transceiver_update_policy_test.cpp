#include "transceiver_update_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }
}

int main ()
{
  using TransceiverUpdatePolicy::Action;
  using TransceiverUpdatePolicy::classify;

  require (Action::ignore_stale_generation == classify (7, 8, 11, 11, true, true),
           "an update from a closed Transceiver generation is ignored first");
  require (Action::ignore_unconfirmed_offline == classify (8, 8, 11, 11, false, false),
           "startup offline state is not promoted into a failure");
  require (Action::accept_current_online == classify (8, 8, 11, 11, true, false),
           "only a current-sequence online state initializes the generation");
  require (Action::accept_current_online == classify (8, 8, 12, 12, true, true),
           "current online snapshots remain accepted after initialization");
  require (Action::ignore_stale_sequence == classify (8, 8, 11, 12, true, true),
           "old-sequence online/PTT snapshots cannot overwrite current state");
  require (Action::report_confirmed_offline == classify (8, 8, 11, 12, false, true),
           "confirmed same-generation offline is terminal despite an old sequence");
  require (Action::report_confirmed_offline == classify (8, 8, 12, 12, false, true),
           "confirmed same-generation current-sequence offline is terminal");
  require (Action::ignore_stale_generation == classify (8, 9, 12, 12, false, true),
           "closing after the offline edge makes a repeated queued edge inert");

  return 0;
}
