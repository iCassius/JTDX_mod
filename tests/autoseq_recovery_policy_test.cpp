#include "autoseq_recovery_policy.hpp"
#include "qsohistory.h"

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  void add (QsoHistory& history, QString const& call, QsoHistory::Status status,
            unsigned time, QString const& report = QStringLiteral ("-10"))
  {
    history.message (call, status, 0, report, QString {}, QStringLiteral ("NA"),
                     QStringLiteral ("W"), time, report, 700, QStringLiteral ("FT8"));
  }

  QsoHistory::Status status_for (QsoHistory& history, QString call)
  {
    QString grid;
    QString report;
    QString mode;
    int rx = 0;
    int tx = 0;
    int count = 0;
    int priority = 0;
    unsigned time = 0;
    return history.autoseq (call, grid, report, rx, tx, time, count, priority, mode);
  }

  QsoHistory history ()
  {
    QsoHistory result;
    result.init ();
    result.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"),
                    QStringLiteral ("OM89"), false);
    return result;
  }

  AutoSeqRecoveryPolicy fresh_ticket (char const * call, std::int64_t recovered,
                                       bool signoff_interrupted = false)
  {
    AutoSeqRecoveryPolicy policy;
    policy.disconnected (true, call, call, signoff_interrupted);
    policy.reconnected_ptt_off (recovered);
    return policy;
  }
}

int main ()
{
  QString const call {QStringLiteral ("ON4IQ")};

  // CAT 重新上线边界和旧批次不得恢复；没有原台消息时票据继续保留。
  auto policy = fresh_ticket ("ON4IQ", 1000);
  policy.begin_decode_batch (1000);
  policy.observe_targeted_message (1000, "ON4IQ", "ON4IQ", 15, false);
  require (!policy.target_seen_in_fresh_batch (1000), "reconnect boundary is stale");
  policy.begin_decode_batch (1001);
  require (!policy.target_seen_in_fresh_batch (1001), "empty fresh batch has no continuation");
  require (policy.pending (), "no-candidate fresh batch keeps the ticket");
  require (!policy.can_process_fresh_batch (false, false, false, false, true, false),
           "PTT-pending recovery batch is blocked without consuming the ticket");
  require (policy.pending (), "blocked recovery batch still keeps the ticket");
  require (policy.can_process_fresh_batch (false, false, false, false, false, false),
           "a later safe fresh batch can be processed with the same ticket");
  policy.observe_targeted_message (1001, "W1AAA", "W1AAA", 15, false);
  require (!policy.target_seen_in_fresh_batch (1001), "another station cannot resume the target");

  // 复合呼号必须原样恢复到 DX 字段；消息匹配可使用规范化基本呼号。
  AutoSeqRecoveryPolicy compound;
  compound.disconnected (true, "OZ7KJ/P", "OZ7KJ", false);
  compound.reconnected_ptt_off (1100);
  compound.begin_decode_batch (1116);
  compound.observe_targeted_message (1116, "OZ7KJ", "OZ7KJ", 45, false);
  require (compound.target () == "OZ7KJ/P", "recovery keeps the full interrupted callsign");
  require (compound.target_seen_in_fresh_batch (1116),
           "base callsign match resumes a compound callsign");

  // 实际 QsoHistory 链：本台先发送报告，对方随后报告后转为 RRREPORT，恢复 Tx4。
  auto rrreport = history ();
  add (rrreport, call, QsoHistory::SREPORT, 30);
  add (rrreport, call, QsoHistory::RREPORT, 45);
  require (status_for (rrreport, call) == QsoHistory::RRREPORT,
           "SREPORT plus RREPORT produces RRREPORT in QsoHistory");
  policy.begin_decode_batch (1016);
  policy.observe_targeted_message (1016, "ON4IQ", "ON4IQ", 45, false);
  require (policy.target_seen_in_fresh_batch (1016), "fresh addressed target is recorded");
  require (policy.action_for_status (status_for (rrreport, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "RRREPORT resumes the existing Tx4 stage");

  // 已有状态机的 RCALL、RREPORT、RRR、RRR73 和 R73 都继续使用对应 Tx2..Tx5。
  auto rcall = history ();
  add (rcall, call, QsoHistory::RCALL, 60);
  require (policy.action_for_status (status_for (rcall, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "RCALL resumes Tx2");
  auto rreport = history ();
  add (rreport, call, QsoHistory::RREPORT, 75);
  require (policy.action_for_status (status_for (rreport, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "RREPORT resumes Tx3");
  auto signoff = history ();
  add (signoff, call, QsoHistory::RREPORT, 90);
  add (signoff, call, QsoHistory::RRR, 105);
  require (status_for (signoff, call) == QsoHistory::RRR, "history reaches RRR");
  require (policy.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "RRR resumes Tx5");
  add (signoff, call, QsoHistory::RRR73, 120);
  require (status_for (signoff, call) == QsoHistory::RRR73, "history reaches RRR73");
  require (policy.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "RRR73 resumes Tx5");
  add (signoff, call, QsoHistory::R73, 135);
  require (status_for (signoff, call) == QsoHistory::R73, "history reaches R73");
  require (policy.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "R73 resumes Tx5 while signoff is not proved complete");

  // Tx5 开始时写入的发送记录可使历史成为 FIN；只有故障时确有 Tx5 在途才重发。
  add (signoff, call, QsoHistory::SRR73, 150);
  require (status_for (signoff, call) == QsoHistory::FIN,
           "first 73 transmission history can already be FIN at Tx start");
  auto partial73 = fresh_ticket ("ON4IQ", 2000, true);
  partial73.begin_decode_batch (2016);
  partial73.observe_targeted_message (2016, "ON4IQ", "ON4IQ", 150, false);
  require (partial73.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::none,
           "fresh report after historical FIN cannot replay Tx5");
  partial73.begin_decode_batch (2031);
  partial73.observe_targeted_message (2031, "ON4IQ", "ON4IQ", 150, true);
  require (partial73.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::arm_existing_stage,
           "in-flight first 73 must repeat Tx5 instead of faking completion");
  auto completed73 = fresh_ticket ("ON4IQ", 3000, false);
  completed73.begin_decode_batch (3016);
  completed73.observe_targeted_message (3016, "ON4IQ", "ON4IQ", 150, false);
  require (completed73.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::none,
           "fresh report after completed history cannot infer completion");
  completed73.begin_decode_batch (3031);
  completed73.observe_targeted_message (3031, "ON4IQ", "ON4IQ", 150, true);
  require (completed73.action_for_status (status_for (signoff, call))
             == AutoSeqRecoveryPolicy::Action::finish_without_tx,
           "completed 73 continuation must not transmit again");

  // 重复故障可在内部清 DX 后保留原票据；用户取消或正常候选提交才会消费它。
  auto repeated = fresh_ticket ("ON4IQ", 4000);
  repeated.disconnected (true, "", "", false);
  require (repeated.target () == "ON4IQ" && repeated.awaiting_reconnect (),
           "repeat fault retains the interrupted target after internal clear");
  repeated.reconnected_ptt_off (5000);
  repeated.begin_decode_batch (5016);
  repeated.observe_targeted_message (5016, "ON4IQ", "ON4IQ", 165, false);
  require (repeated.target_seen_in_fresh_batch (5016), "repeat fault accepts later original continuation");
  repeated.cancel ();
  require (!repeated.pending (), "manual halt disable clear or target edit cancels recovery");

  auto candidate = fresh_ticket ("ON4IQ", 6000);
  candidate.begin_decode_batch (6016);
  require (candidate.pending (), "unrelated CQ is not observed as a continuation");
  candidate.consume_candidate ();
  require (!candidate.pending (), "a real selected normal candidate consumes the ticket");

  return 0;
}
