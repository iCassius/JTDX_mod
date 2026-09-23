#ifndef AUTO_CALL_POLICY_HPP__
#define AUTO_CALL_POLICY_HPP__

#include <QStringList>

namespace AutoCallPolicy
{
  constexpr int rfinStatus = 1;
  constexpr int rcqStatus = 2;
  constexpr int scallStatus = 5;
  constexpr int sreportStatus = 7;

  inline QStringList ft8Fields (QString const& message)
  {
    return message.simplified ().split (' ', Qt::SkipEmptyParts);
  }

  inline bool hasIndependent73 (QString const& message)
  {
    return ft8Fields (message).contains (QStringLiteral ("73"));
  }

  inline bool isCompletionForAutoCall (QString const& message)
  {
    auto const fields = ft8Fields (message);
    return fields.contains (QStringLiteral ("RRR"))
        || fields.contains (QStringLiteral ("RR73"))
        || fields.contains (QStringLiteral ("73"));
  }

  inline bool newGridNeedsReevaluation (bool retainedDxCall, bool hasNewGrid,
                                        bool wholeGridAutoCall,
                                        bool gridBandModeAutoCall, int priority)
  {
    bool const wholeGridPriority = wholeGridAutoCall
        && priority >= 15 && priority <= 16;
    bool const gridBandModePriority = gridBandModeAutoCall
        && priority >= 13 && priority <= 14;
    return retainedDxCall && hasNewGrid
        && (wholeGridPriority || gridBandModePriority);
  }

  // RFIN 是自动新目标首次发送 TX1 时的状态，也必须服从回答 CQ 次数上限。
  inline bool isAnswerCQRetryStatus (int status, bool skipTx1, bool houndMode)
  {
    return !houndMode
        && (status == rfinStatus || status == rcqStatus || status == scallStatus
            || (status == sreportStatus && skipTx1));
  }

  inline bool answerCQRetryLimitReached (int status, bool skipTx1,
                                         bool houndMode, bool counterEnabled,
                                         int counterLimit, int count,
                                         bool replyOtherTerminates)
  {
    return isAnswerCQRetryStatus (status, skipTx1, houndMode)
        && ((counterEnabled && counterLimit <= count) || replyOtherTerminates);
  }

  // Directional CQ eligibility is decided while selecting a candidate. Once
  // a target is active, its stored attempt count belongs to that target and
  // must not be restarted by the direction of a later CQ.
  inline int activeTargetRetryCount (int storedCount)
  {
    return storedCount;
  }

  // Keep the existing five-minute/report-improvement rule in one predicate;
  // QsoHistory chooses a general or special-target failure record by path.
  inline bool suppressAutoCQCandidate (int calledReport, int candidateReport,
                                       unsigned calledTime,
                                       unsigned candidateTime)
  {
    unsigned const elapsed = candidateTime >= calledTime
        ? candidateTime - calledTime
        : candidateTime + 86400u - calledTime;
    return calledReport != -35 && calledReport >= candidateReport
        && elapsed <= 300u;
  }

  inline int receivedReportOrWeakest (QString const& report)
  {
    bool valid = false;
    int const value = report.toInt (&valid);
    return valid ? value : -60;
  }

  // Mixed candidate lists contain both ordinary CQ retries and responses to
  // this station. Failed-CQ cooldown applies only to RCQ/RFIN; genuine QSO
  // replies must pass through to the existing blacklist/direction checks.
  inline bool suppressFailedCQRetry (int status, int calledReport,
                                     QString const& candidateReport,
                                     unsigned calledTime,
                                     unsigned candidateTime)
  {
    return (status == rcqStatus || status == rfinStatus)
        && suppressAutoCQCandidate (calledReport,
                                    receivedReportOrWeakest (candidateReport), calledTime,
                                    candidateTime);
  }

  // 与 readFromStdout 的停发条件保持一致；原始转呼标记不单独触发收尾。
  inline bool replyOtherTerminates (bool replyOther, bool frequencyOverlapsTx,
                                    bool haltTxReplyOther)
  {
    return replyOther && (frequencyOverlapsTx || haltTxReplyOther);
  }

  enum class AnswerCQRetryAction
  {
    none,
    legacyCleanup,
    standbyCleanup
  };

  // 统一识别六组自动起呼优先级；优先级条件只保留在策略层。
  inline bool isConfiguredAutomaticTarget (int priority,
                                           bool newDXCC,
                                           bool newDXCCBandMode,
                                           bool newGrid,
                                           bool newGridBandMode,
                                           bool newCall,
                                           bool newCallBand)
  {
    return (newDXCC && priority >= 22 && priority <= 23)
        || (newDXCCBandMode && priority >= 20 && priority <= 21)
        || (newGrid && priority >= 15 && priority <= 16)
        || (newGridBandMode && priority >= 13 && priority <= 14)
        || (newCall && priority >= 7 && priority <= 8)
        || (newCallBand && priority >= 5 && priority <= 6);
  }

  // 统一回答 CQ 终止判定；界面层只按动作选择对应的收尾语义。
  inline AnswerCQRetryAction answerCQRetryAction (
      int status, bool skipTx1, bool houndMode, bool counterEnabled,
      int counterLimit, int count, bool replyOtherTerminates, bool automaticTarget,
      int priority, bool strictDirectionalCQ)
  {
    if (!answerCQRetryLimitReached (status, skipTx1, houndMode, counterEnabled,
                                    counterLimit, count, replyOtherTerminates))
      return AnswerCQRetryAction::none;
    if (automaticTarget) return AnswerCQRetryAction::standbyCleanup;
    // Every answer-CQ retry limit is a terminal boundary. High-priority
    // targets can otherwise remain selected forever when strict direction is
    // off and their priority falls outside the legacy cleanup ranges.
    (void) priority;
    (void) strictDirectionalCQ;
    return AnswerCQRetryAction::legacyCleanup;
  }

  // A retained DX entry may be replaced by a forced new-grid candidate only
  // after its QSO has actually sent Tx5.  A pending first 73, an active
  // transmission, or an already processed AutoSeq interval must win over
  // candidate replacement.
  inline bool canForceCandidate (bool retainedDxCall, bool processAutoDone,
                                 bool first73Pending, bool transmitting,
                                 bool signoffTransmitted)
  {
    if (processAutoDone || first73Pending || transmitting) return false;
    return !retainedDxCall || signoffTransmitted;
  }

  inline bool logMatchRequired (bool wholeTarget, bool bandTarget,
                                bool bandModeTarget)
  {
    // A band-only target must enter the containing whole-target block so the
    // logbook matcher runs.  Priority selection remains separate.
    return wholeTarget || bandTarget || bandModeTarget;
  }
}

#endif
