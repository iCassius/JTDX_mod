#include "audio_device_policy.hpp"
#include "audio_startup_policy.hpp"

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
  using AudioDeviceSelectionPolicy::Match;
  expect (Match::ExactObject == AudioDeviceSelectionPolicy::choose (true, 3u),
          "exact device wins over duplicate names");
  expect (Match::UniqueName == AudioDeviceSelectionPolicy::choose (false, 1u),
          "unique name is the only fallback");
  expect (Match::AmbiguousName == AudioDeviceSelectionPolicy::choose (false, 2u),
          "duplicate names are rejected");
  expect (Match::NotFound == AudioDeviceSelectionPolicy::choose (false, 0u),
          "missing device is rejected");

  using AudioStartupPolicy::Decision;
  expect (Decision::Wait == AudioStartupPolicy::decide (false, false, false, 100, 500),
          "idle startup waits before timeout");
  expect (Decision::Confirm == AudioStartupPolicy::decide (true, false, false, 0, 500),
          "active state confirms startup even before progress is measurable");
  expect (Decision::Fail == AudioStartupPolicy::decide (false, true, false, 10, 500),
          "stopped startup fails immediately");
  expect (Decision::Fail == AudioStartupPolicy::decide (false, false, true, 10, 500),
          "interrupted startup fails immediately");
  expect (Decision::Fail == AudioStartupPolicy::decide (false, false, false, 500, 500),
          "startup timeout is bounded");
  return 0;
}
