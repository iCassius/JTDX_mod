#ifndef FAKE_IT_STATE_POLICY_HPP__
#define FAKE_IT_STATE_POLICY_HPP__

#include "Radio.hpp"

// State kept by the fake-split decorator.  The wrapped rig exposes one VFO,
// so a TX update can arrive after the requested PTT transition.  The request
// is authoritative for the emulated split transaction: while TX is requested
// the temporary TX dial is exposed, and once RX is requested the saved RX dial
// is exposed even if the wrapped update is late or has no PTT readback.
class FakeItStatePolicy
{
public:
  using Frequency = Radio::Frequency;

  enum class Phase {stable_rx, tx, returning_rx, recovery};

  void request (Frequency rx_frequency, Frequency tx_frequency,
                bool split, bool ptt)
  {
    // Configuration can resend the RX state while the wrapped rig is still
    // returning from Fake It TX (PTT, split/frequency, and mode updates may
    // arrive as separate requests).  Keep that transition armed until the
    // wrapped frequency really confirms the saved RX dial.
    auto const was_tx = phase_ == Phase::tx;
    auto const was_returning = phase_ == Phase::returning_rx;
    rx_frequency_ = rx_frequency;
    tx_frequency_ = tx_frequency;
    split_ = split;
    requested_ptt_ = ptt;
    if (split && ptt) phase_ = Phase::tx;
    else if (was_tx || was_returning) phase_ = Phase::returning_rx;
    else phase_ = Phase::stable_rx;
  }

  Frequency emulatedFrequency (Frequency observed) const
  {
    if (phase_ == Phase::returning_rx || phase_ == Phase::recovery)
      return rx_frequency_;
    if (!split_) return observed;
    return requested_ptt_ && tx_frequency_ ? tx_frequency_ : rx_frequency_;
  }

  Frequency reportedFrequency (Frequency observed)
  {
    if (phase_ == Phase::recovery) return rx_frequency_;
    if (phase_ == Phase::tx) return rx_frequency_;
    if (phase_ == Phase::returning_rx)
      {
        if (observed != rx_frequency_) return rx_frequency_;
        phase_ = Phase::stable_rx;
      }
    // Once the wrapped rig confirms the requested RX dial, follow real VFO
    // changes again.  This is deliberately not a second request path: the
    // next observed value may come from the operator's VFO knob.
    if (observed) rx_frequency_ = observed;
    return observed;
  }

  bool reportedPtt (bool observed) const
  {
    if (phase_ == Phase::recovery || phase_ == Phase::returning_rx)
      return false;
    return phase_ == Phase::tx ? requested_ptt_ : (split_ ? requested_ptt_ : observed);
  }

  void resetToReceive ()
  {
    requested_ptt_ = false;
    split_ = false;
    tx_frequency_ = 0;
    phase_ = Phase::recovery;
  }

  Frequency receiveFrequency () const {return rx_frequency_;}

private:
  Frequency rx_frequency_ {0};
  Frequency tx_frequency_ {0};
  bool split_ {false};
  bool requested_ptt_ {false};
  Phase phase_ {Phase::stable_rx};
};

#endif
