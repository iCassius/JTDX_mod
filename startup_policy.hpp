#ifndef STARTUP_POLICY_HPP__
#define STARTUP_POLICY_HPP__

class StartupPolicy
{
public:
  static constexpr int rigOpenDelayMs () { return 0; }
  static constexpr bool showWindowBeforeRigOpen () { return true; }
};

#endif
