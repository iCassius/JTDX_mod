#ifndef FTX1_CAT_POLL_POLICY_HPP__
#define FTX1_CAT_POLL_POLICY_HPP__

#include <array>
#include <cstdint>

// Qt/Hamlib independent policy for the FTX-1 CAT polling boundary.
// The caller supplies the concrete Hamlib error classification and the
// cached PTT safety state; this header deliberately performs no I/O.
namespace Ftx1CatPollPolicy
{
  enum class Operation
  {
    vfo,
    split,
    rx_frequency,
    other_frequency,
    mode,
    strength,
    power,
    swr,
    ptt,
    protocol_sync,
    count
  };

  enum class Decision
  {
    hard_failure,
    soft_ignore,
    escalate
  };

  struct Context
  {
    bool ftx1 {false};
    bool read_only {true};
    bool transient_error {false};
    bool ptt_known {false};
    bool ptt_intent {false};
    bool ptt_actual {false};
    bool ptt_request_pending {false};
    bool ptt_transition_pending {false};
    // Enable Tx is intentionally not a veto by itself.  It is safe to
    // tolerate a read-only CAT glitch when the actual/requested PTT state is
    // confirmed off, even if the GUI has armed the next automatic cycle.
    bool enable_tx {false};
  };

  inline bool safe_idle (Context const& context)
  {
    return context.ptt_known
      && !context.ptt_intent
      && !context.ptt_actual
      && !context.ptt_request_pending
      && !context.ptt_transition_pending;
  }

  inline bool nonessential_reads_allowed (bool ftx1, bool ptt_known,
                                          bool ptt_intent, bool ptt_actual,
                                          bool ptt_request_pending,
                                          unsigned hold_polls,
                                          bool /*enable_tx*/)
  {
    return !ftx1 || (ptt_known && !ptt_intent && !ptt_actual
                     && !ptt_request_pending && hold_polls == 0);
  }

  // FTX-1 发射期间只允许在 PTT 实际开启、请求已确认且两个过渡保持窗口
  // 均结束后读取功率/SWR 表计；VFO、频率、模式等非必要查询仍由上层暂停。
  inline bool meter_reads_allowed (bool ftx1, bool ptt_known,
                                   bool ptt_intent, bool ptt_actual,
                                   bool ptt_request_pending,
                                   unsigned nonessential_hold_polls,
                                   unsigned ptt_transition_hold_polls)
  {
    return !ftx1 || (ptt_known && ptt_intent && ptt_actual
                     && !ptt_request_pending
                     && nonessential_hold_polls == 0
                     && ptt_transition_hold_polls == 0);
  }

  inline bool legacy_optional_failure (Operation operation,
                                       bool legacy_nonfatal_error,
                                       Context const& context)
  {
    bool optional = false;
    switch (operation)
      {
      case Operation::split:
      case Operation::mode:
      case Operation::strength:
      case Operation::power:
      case Operation::swr:
        optional = true;
        break;
      default:
        break;
      }
    return optional && legacy_nonfatal_error && safe_idle (context);
  }

  // Hamlib's newcat backend can retry a command after receiving a response
  // for another command and still return RIG_OK.  The concrete callback
  // records a monotonic generation; only a generation delta captured inside
  // the current poll is eligible for this operation.
  inline bool has_new_protocol_sync_events (std::uint64_t poll_generation,
                                            std::uint64_t current_generation)
  {
    return current_generation > poll_generation;
  }

  inline bool is_newcat_wrong_reply (bool is_newcat_command,
                                     bool is_wrong_reply)
  {
    return is_newcat_command && is_wrong_reply;
  }

  class State
  {
  public:
    static unsigned constexpr threshold () {return 3;}

    void begin_poll () {poll_failure_seen_ = false;}

    void complete_poll ()
    {
      if (!poll_failure_seen_) overall_failure_streak_ = 0;
      poll_failure_seen_ = false;
    }

    Decision observe_failure (Operation operation, Context const& context)
    {
      if (!context.ftx1 || !context.read_only || !context.transient_error
          || !safe_idle (context))
        {
          return Decision::hard_failure;
        }

      if (!poll_failure_seen_)
        {
          if (overall_failure_streak_ < threshold ()) ++overall_failure_streak_;
          poll_failure_seen_ = true;
        }
      auto& count = failures_[static_cast<unsigned> (operation)];
      if (count < threshold ()) ++count;
      return count >= threshold () || overall_failure_streak_ >= threshold ()
        ? Decision::escalate : Decision::soft_ignore;
    }

    // Returns the number of failures cleared for diagnostic logging.
    unsigned observe_success (Operation operation)
    {
      auto& count = failures_[static_cast<unsigned> (operation)];
      auto const previous = count;
      count = 0;
      return previous;
    }

    unsigned failure_count (Operation operation) const
    {
      return failures_[static_cast<unsigned> (operation)];
    }

    unsigned overall_failure_streak () const {return overall_failure_streak_;}

    void reset ()
    {
      failures_.fill (0);
      overall_failure_streak_ = 0;
      poll_failure_seen_ = false;
    }

  private:
    std::array<unsigned, static_cast<unsigned> (Operation::count)> failures_ {{}};
    unsigned overall_failure_streak_ {0};
    bool poll_failure_seen_ {false};
  };
}

#endif
