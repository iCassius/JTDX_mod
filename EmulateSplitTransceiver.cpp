#include "EmulateSplitTransceiver.hpp"

#include "moc_EmulateSplitTransceiver.cpp"

EmulateSplitTransceiver::EmulateSplitTransceiver (std::unique_ptr<Transceiver> wrapped, QObject * parent)
  : Transceiver {parent}
  , wrapped_ {std::move (wrapped)}
  , rx_frequency_ {0}
  , tx_frequency_ {0}
  , split_ {false}
{
  // Connect update signal of wrapped Transceiver object instance to ours.
  connect (wrapped_.get (), &Transceiver::update, this, &EmulateSplitTransceiver::handle_update);

  // Connect other signals of wrapped Transceiver object to our
  // parent matching signals.
  connect (wrapped_.get (), &Transceiver::resolution, this, &Transceiver::resolution);
  connect (wrapped_.get (), &Transceiver::tciframeswritten, this, &Transceiver::tciframeswritten);
  connect (wrapped_.get (), &Transceiver::tci_mod_active, this, &Transceiver::tci_mod_active);
  connect (wrapped_.get (), &Transceiver::finished, this, &Transceiver::finished);
  connect (wrapped_.get (), &Transceiver::failure, this, &EmulateSplitTransceiver::handle_failure);
}

void EmulateSplitTransceiver::set (TransceiverState const& s, unsigned sequence_number) noexcept
{
#if WSJT_TRACE_CAT
  qDebug () << "EmulateSplitTransceiver::set: state:" << s << "#:" << sequence_number;
#endif
  last_sequence_number_ = sequence_number;
  // save for use in updates
  rx_frequency_ = s.frequency ();
  tx_frequency_ = s.tx_frequency ();
  split_ = s.split ();
  state_policy_.request (rx_frequency_, tx_frequency_, split_, s.ptt ());

  TransceiverState emulated_state {s};
  emulated_state.frequency (state_policy_.emulatedFrequency (s.frequency ()));
  emulated_state.split (false);
  emulated_state.tx_frequency (0);
  wrapped_->set (emulated_state, sequence_number);
}

void EmulateSplitTransceiver::handle_failure (QString const& reason)
{
  // Make the cached client state safe before forwarding the failure.  The
  // wrapped backend can fail without a final PTT readback, so a TX VFO
  // update must not remain visible as the next RX dial.
  auto const rx_frequency = state_policy_.receiveFrequency ();
  state_policy_.resetToReceive ();
  TransceiverState recovery;
  recovery.frequency (rx_frequency);
  recovery.ptt (false);
  recovery.split (false);
  recovery.tx_frequency (0);
  Q_EMIT update (recovery, last_sequence_number_);
  Q_EMIT failure (reason);
}

void EmulateSplitTransceiver::handle_update (TransceiverState const& state,
                                             unsigned sequence_number)
{
#if WSJT_TRACE_CAT
  qDebug () << "EmulateSplitTransceiver::handle_update: from wrapped:" << state;
#endif

  if (state.split ())
    {
      Q_EMIT failure (tr ("Emulated split mode requires rig to be in simplex mode"));
    }
  else
    {
      TransceiverState new_state {state};
      // Do not infer the emulated VFO from a wrapped PTT poll.  In
      // particular, a rig with PTT type None may report a stale PTT state,
      // and a late TX-frequency update must never become the next RX dial.
      new_state.frequency (state_policy_.reportedFrequency (state.frequency ()));
      new_state.ptt (state_policy_.reportedPtt (state.ptt ()));

      // These are always what was requested in prior set state operation
      new_state.tx_frequency (tx_frequency_);
      new_state.split (split_);

#if WSJT_TRACE_CAT
      qDebug () << "EmulateSplitTransceiver::handle_update: signalling:" << state;
#endif

      // signal emulated state
      Q_EMIT update (new_state, sequence_number);
    }
}
