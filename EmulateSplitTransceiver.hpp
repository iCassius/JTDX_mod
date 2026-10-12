#ifndef EMULATE_SPLIT_TRANSCEIVER_HPP__
#define EMULATE_SPLIT_TRANSCEIVER_HPP__

#include <memory>

#include "Transceiver.hpp"
#include "fake_it_state_policy.hpp"

// Emulate Split Transceiver
//
// Helper decorator class that encapsulates  the emulation of split TX
// operation.
//
// Responsibilities
//
//  Delegates  all  but setting  of  other  (split) frequency  to  the
//  wrapped Transceiver instance. Also routes failure signals from the
//  wrapped Transceiver instance to this instances failure signal.
//
//  Intercepts status  updates from  the wrapped  Transceiver instance
//  and re-signals it with the emulated status.
//
//  Generates a status update signal if the other (split) frequency is
//  changed, this is necessary  since the wrapped transceiver instance
//  never receives other frequency changes.
//
class EmulateSplitTransceiver final
  : public Transceiver
{
  Q_OBJECT

public:
  // takes ownership of wrapped Transceiver
  explicit EmulateSplitTransceiver (std::unique_ptr<Transceiver> wrapped,
                                    QObject * parent = nullptr);

  void set (TransceiverState const&,
            unsigned sequence_number,
            PttAdmissionBlock::Token ptt_admission_token) noexcept override;
  void refresh_read_only (quint64 request_id) noexcept override;
  bool supports_health_observation () const noexcept override;

  // forward everything else to wrapped Transceiver
  void start (unsigned sequence_number, JTDXDateTime* jtdxtime) noexcept override {wrapped_->start (sequence_number,jtdxtime);}
  void stop () noexcept override {wrapped_->stop ();}

protected:
  void bind_delegated_ptt_admission (
    std::shared_ptr<PttAdmissionBlock> const&) noexcept override;
  void move_delegates_to_thread (QThread *) noexcept override;

private:
  void handle_update (TransceiverState const&, unsigned seqeunce_number);
  void handle_failure (QString const& reason);

  std::unique_ptr<Transceiver> wrapped_;
  FakeItStatePolicy state_policy_;
  Frequency rx_frequency_;        // requested Rx frequency
  Frequency tx_frequency_;        // requested Tx frequency
  bool split_; // requested split state
  bool last_observed_ptt_ {false};
  unsigned last_sequence_number_ {0};
};

#endif
