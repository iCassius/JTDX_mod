#include "Transceiver.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * message)
  {
    if (!condition)
      {
        std::cerr << message << '\n';
        std::exit (EXIT_FAILURE);
      }
  }
}

int main ()
{
  Transceiver::TransceiverState control;
  Transceiver::TransceiverState observed {control};
  observed.ptt_readback_required (true);
  observed.ptt_readback_known (true);

  require (control != observed,
           "readback validity/capability changes must be signalled to clients");
  require (control.same_control_state (observed),
           "readback metadata must not alter PollingTransceiver control stabilization");
  observed.ptt_readback_serial (1);
  require (control != observed,
           "each successful PTT readback serial must be observable by clients");
  require (control.same_control_state (observed),
           "readback serial changes must not consume command-stabilization polls");
  observed.ptt (true);
  require (!control.same_control_state (observed),
           "an actual PTT control-state change remains visible to stabilization");
  return EXIT_SUCCESS;
}
