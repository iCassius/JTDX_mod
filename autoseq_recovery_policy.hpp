#ifndef AUTOSEQ_RECOVERY_POLICY_HPP__
#define AUTOSEQ_RECOVERY_POLICY_HPP__

#include <cstdint>
#include <string>

// CAT 硬故障后的 AutoSeq 纯协调器：只记录中断 QSO 身份和事件顺序，
// 实际 CAT、界面、解码和发射操作仍由 MainWindow 负责。
class AutoSeqRecoveryPolicy
{
public:
  // 状态值对应 QsoHistory::Status，避免这个小型生产协调器依赖 Qt/QsoHistory。
  enum class Action
  {
    none,
    arm_existing_stage,
    finish_without_tx
  };

  void disconnected (bool preserve_user_intent, std::string const& target,
                     std::string const& target_match_key,
                     bool signoff_tx_interrupted)
  {
    if (!preserve_user_intent)
      {
        cancel ();
        return;
      }

    // MainWindow 可能已为恢复主动清空 DX；重复故障前均保留原目标，直到
    // 选择候选、确认结束或用户明确取消该恢复票据。
    if (target_.empty () && !target.empty ())
      {
        target_ = target;
        target_match_key_ = target_match_key.empty () ? target : target_match_key;
        signoff_tx_interrupted_ = signoff_tx_interrupted;
      }

    // 空闲 AutoSeq 故障没有可续联的上下文，不进入自动恢复流程。
    if (target_.empty ())
      {
        cancel ();
        return;
      }

    preserve_user_intent_ = true;
    awaiting_reconnect_ = true;
    awaiting_fresh_batch_ = false;
    recovered_at_ = 0;
    reset_batch ();
  }

  // 仅在收发机已经报告在线且实际 PTT 已确认关闭后调用。FTX-1 成功启动
  // 必须先通过 PTT-first poll 的 RIG_OK，才会到达这里。
  void reconnected_ptt_off (std::int64_t recovered_at)
  {
    if (preserve_user_intent_ && awaiting_reconnect_)
      {
        awaiting_reconnect_ = false;
        awaiting_fresh_batch_ = true;
        recovered_at_ = recovered_at;
        reset_batch ();
      }
  }

  void begin_decode_batch (std::int64_t decode_started_at)
  {
    batch_started_at_ = decode_started_at;
    target_seen_in_batch_ = false;
    target_completion_seen_in_batch_ = false;
    target_receive_time_ = 0;
  }

  // 调用方只在标准报文已确认发给本台后传入基准呼号，避免把历史
  // QsoHistory 状态、自由文本或无关 CQ 当作续联。
  void observe_targeted_message (std::int64_t decode_started_at,
                                 std::string const& sender,
                                 std::string const& sender_match_key,
                                 unsigned receive_time,
                                 bool is_completion_message)
  {
    if (awaiting_fresh_batch_ && is_fresh_batch (decode_started_at)
        && (sender == target_
            || (!sender_match_key.empty () && sender_match_key == target_match_key_)))
      {
        target_seen_in_batch_ = true;
        target_completion_seen_in_batch_ = target_completion_seen_in_batch_
          || is_completion_message;
        target_receive_time_ = receive_time;
      }
  }

  bool pending () const
  {
    return preserve_user_intent_ && (awaiting_reconnect_ || awaiting_fresh_batch_);
  }

  // MainWindow 使用这个纯门控判断整批恢复处理是否仍处于安全空闲状态。
  // 它不改变票据；被阻断的批次由调用方立即返回，下一批仍可重新判断。
  bool can_process_fresh_batch (bool hound_mode, bool wspr_mode,
                                bool transmitting, bool tune,
                                bool ptt_pending, bool first_73_pending) const
  {
    return !hound_mode && !wspr_mode && !transmitting && !tune
      && !ptt_pending && !first_73_pending;
  }

  bool awaiting_reconnect () const {return awaiting_reconnect_;}
  bool awaiting_fresh_batch () const {return awaiting_fresh_batch_;}
  bool is_fresh_batch (std::int64_t decode_started_at) const
  {
    return awaiting_fresh_batch_ && decode_started_at > recovered_at_;
  }
  bool target_seen_in_fresh_batch (std::int64_t decode_started_at) const
  {
    return is_fresh_batch (decode_started_at)
      && batch_started_at_ == decode_started_at && target_seen_in_batch_;
  }
  bool target_completion_seen_in_fresh_batch (std::int64_t decode_started_at) const
  {
    return target_seen_in_fresh_batch (decode_started_at)
      && target_completion_seen_in_batch_;
  }
  unsigned target_receive_time () const {return target_receive_time_;}
  std::string const& target () const {return target_;}
  bool signoff_tx_interrupted () const {return signoff_tx_interrupted_;}
  std::int64_t recovered_at () const {return recovered_at_;}

  Action action_for_status (int status) const
  {
    // RCALL、RREPORT、RRREPORT、RRR、RRR73 和尚未完成首个 73 的 R73
    // 沿用既有 Tx2..Tx5。若首个 73 在 CAT 故障中断，即使历史已在 TX
    // 起始处写成 FIN，也只能在新的原台结束报文后重发 Tx5，不得伪装完成。
    switch (status)
      {
      case 4:   // RCALL
      case 6:   // RREPORT
      case 8:   // RRREPORT
      case 10:  // RRR
      case 12:  // RRR73
        return Action::arm_existing_stage;
      case 14:  // R73
        return Action::arm_existing_stage;
      case 13:  // SRR73
      case 15:  // S73
      case 16:  // FIN
        // 历史 FIN 可能只是 Tx5 刚开始时写入。只有本批原台实际给出
        // RRR、RR73 或 73，才能重发未完成的 Tx5 或确认此前已完成。
        if (!target_completion_seen_in_batch_) return Action::none;
        return signoff_tx_interrupted_ ? Action::arm_existing_stage
                                        : Action::finish_without_tx;
      default:
        return Action::none;
      }
  }

  void consume_candidate () {cancel ();}

  void cancel ()
  {
    preserve_user_intent_ = false;
    awaiting_reconnect_ = false;
    awaiting_fresh_batch_ = false;
    recovered_at_ = 0;
    target_.clear ();
    target_match_key_.clear ();
    signoff_tx_interrupted_ = false;
    reset_batch ();
  }

private:
  void reset_batch ()
  {
    batch_started_at_ = 0;
    target_seen_in_batch_ = false;
    target_completion_seen_in_batch_ = false;
    target_receive_time_ = 0;
  }

  bool preserve_user_intent_ {false};
  bool awaiting_reconnect_ {false};
  bool awaiting_fresh_batch_ {false};
  bool signoff_tx_interrupted_ {false};
  std::int64_t recovered_at_ {0};
  std::int64_t batch_started_at_ {0};
  bool target_seen_in_batch_ {false};
  bool target_completion_seen_in_batch_ {false};
  unsigned target_receive_time_ {0};
  std::string target_;
  std::string target_match_key_;
};

#endif
