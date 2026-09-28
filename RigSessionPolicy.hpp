#ifndef RIG_SESSION_POLICY_HPP__
#define RIG_SESSION_POLICY_HPP__

namespace RigSessionPolicy
{
  enum class Purpose
  {
    runtime,
    configuration_test
  };

  enum class FailureAction
  {
    ignore_stale,
    forward_to_runtime,
    report_to_configuration
  };

  constexpr FailureAction failure_action (unsigned long long failure_generation,
                                           unsigned long long current_generation,
                                           Purpose purpose) noexcept
  {
    return failure_generation != current_generation
      ? FailureAction::ignore_stale
      : Purpose::runtime == purpose
        ? FailureAction::forward_to_runtime
        : FailureAction::report_to_configuration;
  }

  constexpr Purpose purpose_after_reuse (Purpose active_purpose,
                                         Purpose caller_purpose) noexcept
  {
    return Purpose::runtime == caller_purpose
      ? Purpose::runtime
      : active_purpose;
  }
}

#endif
