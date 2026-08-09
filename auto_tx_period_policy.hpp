#ifndef AUTO_TX_PERIOD_POLICY_HPP__
#define AUTO_TX_PERIOD_POLICY_HPP__

#include <cmath>

// The receive slot is represented as seconds from the UTC day by QsoHistory.
// The returned value is the opposite slot used for the next transmission.
namespace AutoTxPeriodPolicy
{
  inline bool txFirstForReceiveTime (double receiveTimeSeconds, double trPeriodSeconds)
  {
    if (!std::isfinite (receiveTimeSeconds) || !std::isfinite (trPeriodSeconds)
        || trPeriodSeconds <= 0.0)
      {
        return false;
      }

    auto const cycle = 2.0 * trPeriodSeconds;
    auto phase = std::fmod (receiveTimeSeconds, cycle);
    if (phase < 0.0) phase += cycle;

    // At the exact receive boundary the opposite transmission slot is the
    // non-first slot, matching the existing manual double-click semantics.
    auto const epsilon = 1.0e-9;
    return phase > epsilon && phase < cycle - epsilon;
  }

  inline bool changed (bool currentTxFirst, double receiveTimeSeconds, double trPeriodSeconds)
  {
    return currentTxFirst != txFirstForReceiveTime (receiveTimeSeconds, trPeriodSeconds);
  }
}

#endif
