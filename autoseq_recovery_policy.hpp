#ifndef AUTOSEQ_RECOVERY_POLICY_HPP__
#define AUTOSEQ_RECOVERY_POLICY_HPP__

#include <cstdint>

// Pure gate for resuming an automatic sequence after a hard rig failure.
// It never chooses a candidate and never performs a transmit operation.
class AutoSeqRecoveryPolicy
{
public:
  void disconnected (bool preserve_user_intent)
  {
    preserve_user_intent_ = preserve_user_intent;
    awaiting_reconnect_ = preserve_user_intent;
    awaiting_fresh_candidate_ = false;
    recovered_at_ = 0;
  }

  void reconnected (std::int64_t recovered_at)
  {
    if (awaiting_reconnect_)
      {
        awaiting_reconnect_ = false;
        awaiting_fresh_candidate_ = true;
        recovered_at_ = recovered_at;
      }
  }

  bool can_arm (std::int64_t decode_started_at, bool fresh_candidate) const
  {
    if (!preserve_user_intent_) return true;
    if (awaiting_reconnect_ || !awaiting_fresh_candidate_) return false;
    return fresh_candidate && decode_started_at > recovered_at_;
  }

  void consume_candidate ()
  {
    preserve_user_intent_ = false;
    awaiting_reconnect_ = false;
    awaiting_fresh_candidate_ = false;
    recovered_at_ = 0;
  }

  void cancel ()
  {
    preserve_user_intent_ = false;
    awaiting_reconnect_ = false;
    awaiting_fresh_candidate_ = false;
    recovered_at_ = 0;
  }

  bool pending () const
  {
    return preserve_user_intent_ && (awaiting_reconnect_ || awaiting_fresh_candidate_);
  }

  std::int64_t recovered_at () const {return recovered_at_;}

private:
  bool preserve_user_intent_ {false};
  bool awaiting_reconnect_ {false};
  bool awaiting_fresh_candidate_ {false};
  std::int64_t recovered_at_ {0};
};

#endif
