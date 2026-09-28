#include "RigSessionPolicy.hpp"

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
  using RigSessionPolicy::FailureAction;
  using RigSessionPolicy::Purpose;

  require (FailureAction::forward_to_runtime
             == RigSessionPolicy::failure_action (7, 7, Purpose::runtime),
           "current runtime session failures are forwarded to MainWindow");
  require (FailureAction::report_to_configuration
             == RigSessionPolicy::failure_action (7, 7, Purpose::configuration_test),
           "current configuration-test failures remain local");
  require (FailureAction::ignore_stale
             == RigSessionPolicy::failure_action (6, 7, Purpose::runtime),
           "stale failures are ignored regardless of old session purpose");
  require (Purpose::runtime
             == RigSessionPolicy::purpose_after_reuse (Purpose::configuration_test,
                                                        Purpose::runtime),
           "runtime callers promote an active configuration-test session");
  require (Purpose::runtime
             == RigSessionPolicy::purpose_after_reuse (Purpose::runtime,
                                                        Purpose::configuration_test),
           "configuration-test callers preserve an active runtime session");
  return 0;
}
