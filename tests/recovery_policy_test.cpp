#include "recovery_policy.hpp"
#include "startup_policy.hpp"

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
  RecoveryPolicy policy;
  require (0 == policy.attempts (), "new policy must have no attempts");
  require (2000 == policy.nextDelayMs (), "first retry delay must be 2 seconds");
  require (1 == policy.attempts (), "first retry must be counted");
  require (5000 == policy.nextDelayMs (), "second retry delay must be 5 seconds");
  require (15000 == policy.nextDelayMs (), "third retry delay must be 15 seconds");
  require (-1 == policy.nextDelayMs (), "retry sequence must be bounded");
  require (3 == policy.attempts (), "exhaustion must not add phantom attempts");

  policy.reset ();
  require (0 == policy.attempts (), "reset must clear the attempt count");
  require (2000 == policy.nextDelayMs (), "reset must restore the first delay");

  require (StartupPolicy::showWindowBeforeRigOpen (),
           "startup must make the main window available before rig open");
  require (0 == StartupPolicy::rigOpenDelayMs (), "rig open is deferred to the event loop");
  return 0;
}
