// -*- Mode: C++ -*-
#ifndef RECOVERY_POLICY_HPP__
#define RECOVERY_POLICY_HPP__

// Small, Qt-independent retry policy used for transient rig-control failures.
// A failure is never reported as recovered until a real online state arrives.
class RecoveryPolicy
{
public:
  int nextDelayMs ()
  {
    switch (m_attempts++)
      {
      case 0: return 2000;
      case 1: return 5000;
      case 2: return 15000;
      default:
        --m_attempts;
        return -1;
      }
  }

  void reset () {m_attempts = 0;}
  int attempts () const {return m_attempts;}

private:
  int m_attempts {0};
};

#endif
