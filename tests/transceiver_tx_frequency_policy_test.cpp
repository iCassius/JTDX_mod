#include "transceiver_tx_frequency_policy.hpp"

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
  using TransceiverTxFrequencyPolicy::should_send;

  require (!should_send (true, true, 14074000ULL, 14074000ULL,
                         0LL, 0LL, false),
           "same split, calibrated frequency and offset does not resend SET");
  require (should_send (true, true, 14074000ULL, 14074010ULL,
                        0LL, 0LL, false),
           "calibration change in final TX frequency sends SET");
  require (should_send (true, true, 14074000ULL, 14074000ULL,
                        100LL, 150LL, false),
           "offset change sends SET even when final frequency is unchanged");
  require (should_send (true, true, 14074000ULL, 14074000ULL,
                        0LL, 0LL, true),
           "effective cached mode change is not swallowed by frequency deduplication");
  require (should_send (true, false, 14074000ULL, 0ULL,
                        150LL, 150LL, false),
           "zero TX frequency preserves split-off command semantics");
  require (!should_send (false, false, 0ULL, 0ULL,
                         150LL, 0LL, false),
           "inactive split ignores an offset that cannot affect the zero TX target");
  require (should_send (false, true, 0ULL, 14074000ULL,
                        0LL, 0LL, false),
           "new split target sends SET");
  return 0;
}
