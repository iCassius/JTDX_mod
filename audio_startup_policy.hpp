#ifndef AUDIO_STARTUP_POLICY_HPP__
#define AUDIO_STARTUP_POLICY_HPP__

#include <cstdint>

namespace AudioStartupPolicy
{
  enum class Decision
  {
    Wait,
    Confirm,
    Fail
  };

  inline Decision decide (bool active, bool stopped, bool interrupted,
                          std::int64_t elapsedMs, std::int64_t timeoutMs)
  {
    if (active) return Decision::Confirm;
    if (stopped || interrupted) return Decision::Fail;
    if (elapsedMs >= timeoutMs) return Decision::Fail;
    return Decision::Wait;
  }
}

#endif
