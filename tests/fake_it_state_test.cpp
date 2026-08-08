#include "fake_it_state_policy.hpp"

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
}

int main ()
{
  FakeItStatePolicy policy;
  constexpr Radio::Frequency rx {21074000};
  constexpr Radio::Frequency tx {21075000};

  policy.request (rx, tx, true, true);
  require (tx == policy.emulatedFrequency (rx),
           "Fake It must select the temporary TX dial during TX");
  require (rx == policy.reportedFrequency (tx),
           "Fake It must keep the original RX dial visible during TX");
  require (policy.reportedPtt (false),
           "requested PTT must cover a missing PTT readback during TX");

  policy.request (rx, tx, true, false);
  policy.request (rx, tx, true, false);
  require (rx == policy.emulatedFrequency (tx),
           "Fake It must command the saved RX dial after TX");
  require (rx == policy.reportedFrequency (tx),
           "repeated RX requests must not accept a late TX update");
  require (rx == policy.reportedFrequency (rx),
           "the RX-frequency update must confirm the return transition");
  require (!policy.reportedPtt (true),
           "requested RX must cover a late PTT-on update");

  constexpr Radio::Frequency manual_rx {21074123};
  require (manual_rx == policy.reportedFrequency (manual_rx),
           "manual RX VFO changes must remain effective in RX state");

  policy.request (rx, tx, true, true);
  policy.resetToReceive ();
  require (rx == policy.reportedFrequency (tx),
           "error recovery must keep reporting the original RX dial");
  require (!policy.reportedPtt (true),
           "error recovery must report RX even without a final PTT readback");
  return 0;
}
