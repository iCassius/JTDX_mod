#ifndef PTT_START_POLICY_HPP__
#define PTT_START_POLICY_HPP__

#include <cstdint>
#include <string>

// Correlates a PTT observation and delayed audio start with one transmit
// request and one Configuration rig generation. This does not retry PTT.
class PttStartPolicy
{
public:
  struct Token
  {
    std::uint64_t serial {0};
    std::uint64_t generation {0};
    std::uint64_t readback_serial {0};
    bool requires_fresh_readback {false};
  };

  struct RequestContext
  {
    std::string logical_target;
    std::string band;
    std::string mode;
    std::string tx_mode;
    std::int64_t dial_frequency {0};
    int tx_audio_frequency {0};
    int tx_index {0};
    int qso_phase {0};
    // Identity of the selected encoded TX1-TX6/gen/free-text payload.
    // Tune and WSPR leave this empty because they use independent generators.
    std::string tx_payload_identity;
    bool tune {false};
  };

  enum class Observation
  {
    stale,
    waiting_for_readback,
    duplicate_readback,
    waiting_for_ptt_on,
    audio_armed,
    audio_already_started,
    ptt_went_off_after_confirmation
  };

  Token begin (std::uint64_t generation, std::uint64_t readback_serial = 0,
               bool requires_fresh_readback = false)
  {
    if (++next_serial_ == 0) ++next_serial_;
    token_.serial = next_serial_;
    token_.generation = generation;
    token_.readback_serial = readback_serial;
    token_.requires_fresh_readback = requires_fresh_readback;
    active_ = true;
    ptt_on_confirmed_ = false;
    audio_armed_ = false;
    audio_started_ = false;
    last_readback_serial_seen_ = readback_serial;
    return token_;
  }

  bool matches (Token token, std::uint64_t generation) const
  {
    return active_ && token.serial != 0 && token.serial == token_.serial
      && token.generation == token_.generation && generation == token_.generation;
  }

  static bool ready_for_request (bool readback_required, bool online,
                                 bool ptt_known, bool ptt_on,
                                 bool state_is_current_generation,
                                 std::uint64_t readback_serial = 0)
  {
    return !readback_required
      || (online && ptt_known && !ptt_on && state_is_current_generation
          && readback_serial != 0);
  }

  static std::string logical_target (std::string target, bool calling_cq)
  {
    return calling_cq ? std::string {} : target;
  }

  static bool same_request_context (RequestContext const& requested,
                                    RequestContext const& current)
  {
    if (requested.tune || current.tune) return requested.tune && current.tune;
    return requested.logical_target == current.logical_target
      && requested.band == current.band
      && requested.mode == current.mode
      && requested.tx_mode == current.tx_mode
      && requested.dial_frequency == current.dial_frequency
      && requested.tx_audio_frequency == current.tx_audio_frequency
      && requested.tx_index == current.tx_index
      && requested.qso_phase == current.qso_phase
      && requested.tx_payload_identity == current.tx_payload_identity;
  }

  static bool may_start_request (bool slot_active, bool tune_stop_pending)
  {
    return slot_active && !tune_stop_pending;
  }

  static bool should_cancel_pending_audio (bool enable_tx, bool request_pending,
                                           bool tune_active)
  {
    return !enable_tx && request_pending && !tune_active;
  }

  static bool should_cancel_request_at_slot_end (bool tx_slot_active,
                                                  bool tune_active,
                                                  bool request_pending)
  {
    return !tx_slot_active && !tune_active && request_pending;
  }

  static bool fresh_auto_decision (std::uint64_t rejected_batch,
                                   std::uint64_t current_batch, bool auto_seq,
                                   bool manual_decode, bool disk_decode)
  {
    return auto_seq && !manual_decode && !disk_decode
      && current_batch > rejected_batch;
  }

  static bool may_rearm_initial_owner (bool fresh_decision, bool owner_active,
                                       bool same_owner, bool initial_candidate,
                                       bool recovery_ticket_pending,
                                       bool operator_hold)
  {
    return fresh_decision && owner_active && same_owner && initial_candidate
      && !recovery_ticket_pending && !operator_hold;
  }

  Observation observe (Token token, std::uint64_t generation,
                       bool online, bool ptt_known, bool ptt_on,
                       std::uint64_t readback_serial = 0)
  {
    if (!matches (token, generation)) return Observation::stale;
    if (!online || !ptt_known
        || (token.requires_fresh_readback && readback_serial <= token.readback_serial))
      {
        audio_armed_ = false;
        return Observation::waiting_for_readback;
      }
    if (token.requires_fresh_readback && readback_serial <= last_readback_serial_seen_)
      return Observation::duplicate_readback;
    if (token.requires_fresh_readback) last_readback_serial_seen_ = readback_serial;
    if (ptt_on)
      {
        ptt_on_confirmed_ = true;
        if (audio_started_) return Observation::audio_already_started;
        audio_armed_ = true;
        return Observation::audio_armed;
      }
    audio_armed_ = false;
    return ptt_on_confirmed_ ? Observation::ptt_went_off_after_confirmation
                             : Observation::waiting_for_ptt_on;
  }

  bool consume_audio (Token token, std::uint64_t generation,
                      bool online, bool ptt_known, bool ptt_on,
                      std::uint64_t readback_serial = 0)
  {
    if (!matches (token, generation) || !audio_armed_ || audio_started_
        || !online || !ptt_known || !ptt_on
        || (token.requires_fresh_readback
            && (readback_serial <= token.readback_serial
                || readback_serial < last_readback_serial_seen_)))
      return false;
    audio_armed_ = false;
    audio_started_ = true;
    return true;
  }

  bool ptt_on_confirmed (Token token, std::uint64_t generation) const
  {
    return matches (token, generation) && ptt_on_confirmed_;
  }

  bool active () const { return active_; }

  void cancel ()
  {
    active_ = false;
    ptt_on_confirmed_ = false;
    audio_armed_ = false;
    audio_started_ = false;
    last_readback_serial_seen_ = 0;
  }

private:
  Token token_ {};
  std::uint64_t next_serial_ {0};
  bool active_ {false};
  bool ptt_on_confirmed_ {false};
  bool audio_armed_ {false};
  bool audio_started_ {false};
  std::uint64_t last_readback_serial_seen_ {0};
};

#endif
