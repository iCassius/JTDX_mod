#include "auto_tx_period_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool actual, char const * description)
  {
    if (!actual)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }
}

int main ()
{
  // FT8: receive 00/30 -> transmit 15/45, receive 15/45 -> transmit 00/30.
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (0.0, 15.0), "FT8 receive 00");
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (30.0, 15.0), "FT8 receive 30");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (15.0, 15.0), "FT8 receive 15");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (45.0, 15.0), "FT8 receive 45");

  // A candidate after an automatic stop must be able to change the inherited
  // period, including at a UTC day boundary.
  expect (AutoTxPeriodPolicy::changed (true, 30.0, 15.0), "automatic stop switches to opposite period");
  expect (!AutoTxPeriodPolicy::changed (false, 30.0, 15.0), "unchanged period is not reported as changed");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (86385.0, 15.0), "cross-midnight receive 23:59:45");
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (86400.0, 15.0), "cross-midnight receive 00:00");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (86415.0, 15.0), "cross-midnight receive 00:00:15");

  // FT4 has 7.5-second slots; the policy remains precise even though the
  // current QsoHistory storage is an integer-second field.
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (0.0, 7.5), "FT4 receive 00");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (7.5, 7.5), "FT4 receive 07.5");
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (15.0, 7.5), "FT4 receive 15");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (22.5, 7.5), "FT4 receive 22.5");

  // Compatibility with the manual double-click formula:
  // m_txFirst = (time % (2 * TRperiod)) != 0.
  expect (!AutoTxPeriodPolicy::txFirstForReceiveTime (60.0, 15.0), "manual boundary semantics");
  expect (AutoTxPeriodPolicy::txFirstForReceiveTime (75.0, 15.0), "manual non-boundary semantics");
  return 0;
}
