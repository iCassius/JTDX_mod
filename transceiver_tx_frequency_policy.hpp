#ifndef TRANSCEIVER_TX_FREQUENCY_POLICY_HPP__
#define TRANSCEIVER_TX_FREQUENCY_POLICY_HPP__

namespace TransceiverTxFrequencyPolicy
{
  template <typename Frequency, typename Offset>
  constexpr bool should_send (bool current_split, bool requested_split,
                              Frequency current_tx_frequency,
                              Frequency requested_tx_frequency,
                              Offset current_offset, Offset requested_offset,
                              bool mode_change) noexcept
  {
    return current_split != requested_split
      || current_tx_frequency != requested_tx_frequency
      || (requested_split && current_offset != requested_offset)
      || mode_change;
  }
}

#endif
