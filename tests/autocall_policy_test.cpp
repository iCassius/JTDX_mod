#include "qsohistory.h"
#include "auto_call_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void expect(bool actual, char const * description)
{
  if (!actual) {
    std::cerr << "failed: " << description << '\n';
    std::exit(1);
  }
}
}

int main()
{
  // The legacy AutoSeq selector must be unchanged when no rare-target
  // option is active.
  expect(QsoHistory::autoCallPriorityAllowed(17, 0), "legacy priority 17");
  expect(QsoHistory::autoCallPriorityAllowed(19, 0), "legacy priority 19");
  expect(!QsoHistory::autoCallPriorityAllowed(22, 0), "legacy excludes new DXCC");

  expect(QsoHistory::autoCallPriorityAllowed(22, QsoHistory::AutoCallNewDXCC), "new DXCC");
  expect(QsoHistory::autoCallPriorityAllowed(23, QsoHistory::AutoCallNewDXCC), "new DXCC band");
  expect(!QsoHistory::autoCallPriorityAllowed(20, QsoHistory::AutoCallNewDXCC), "DXCC option excludes band/mode");

  expect(QsoHistory::autoCallPriorityAllowed(20, QsoHistory::AutoCallNewDXCCBandMode), "DXCC new band/mode");
  expect(QsoHistory::autoCallPriorityAllowed(21, QsoHistory::AutoCallNewDXCCBandMode), "DXCC new mode/band");
  expect(!QsoHistory::autoCallPriorityAllowed(22, QsoHistory::AutoCallNewDXCCBandMode), "band/mode excludes new DXCC");

  expect(QsoHistory::autoCallPriorityAllowed(16, QsoHistory::AutoCallNewGrid), "new grid");
  expect(QsoHistory::autoCallPriorityAllowed(15, QsoHistory::AutoCallNewGrid), "new grid without LOTW");
  expect(!QsoHistory::autoCallPriorityAllowed(14, QsoHistory::AutoCallNewGrid), "new grid excludes band/mode");

  expect(QsoHistory::autoCallPriorityAllowed(13, QsoHistory::AutoCallNewGridBandMode), "new grid band/mode");
  expect(QsoHistory::autoCallPriorityAllowed(14, QsoHistory::AutoCallNewGridBandMode), "new grid band/mode with LOTW");
  expect(!QsoHistory::autoCallPriorityAllowed(15, QsoHistory::AutoCallNewGridBandMode), "grid band/mode excludes new grid");
  unsigned const bothGridOptions = QsoHistory::AutoCallNewGrid | QsoHistory::AutoCallNewGridBandMode;
  expect(QsoHistory::autoCallPriorityAllowed(13, bothGridOptions), "both grid options include band/mode");
  expect(QsoHistory::autoCallPriorityAllowed(16, bothGridOptions), "both grid options include new grid");
  expect(!QsoHistory::autoCallPriorityAllowed(12, bothGridOptions), "grid options exclude unrelated priority");
  expect(!QsoHistory::autoCallPriorityAllowed(17, bothGridOptions), "grid options exclude wanted country");

  expect(QsoHistory::autoCallPriorityAllowed(7, QsoHistory::AutoCallNewCall), "new callsign");
  expect(QsoHistory::autoCallPriorityAllowed(8, QsoHistory::AutoCallNewCall), "new callsign with LOTW");
  expect(!QsoHistory::autoCallPriorityAllowed(9, QsoHistory::AutoCallNewCall), "new callsign excludes new prefix");

  expect(QsoHistory::autoCallPriorityAllowed(5, QsoHistory::AutoCallNewCallBand), "new callsign band");
  expect(QsoHistory::autoCallPriorityAllowed(6, QsoHistory::AutoCallNewCallBand), "new callsign band with LOTW");
  expect(!QsoHistory::autoCallPriorityAllowed(7, QsoHistory::AutoCallNewCallBand), "new callsign band excludes new callsign");

  expect(AutoCallPolicy::hasIndependent73 ("W1ABC BI7KGD 73"), "independent 73 field");
  expect(AutoCallPolicy::isCompletionForAutoCall ("W1ABC BI7KGD RRR"), "RRR completion field");
  expect(AutoCallPolicy::isCompletionForAutoCall ("W1ABC BI7KGD RR73"), "RR73 completion field");
  expect(!AutoCallPolicy::hasIndependent73 ("W1ABC BI7KGD 73ABC"), "call containing 73 is not 73 field");
  expect(!AutoCallPolicy::hasIndependent73 ("W1ABC BI7KGD <73>"), "decorated text is not 73 field");
  expect(AutoCallPolicy::logMatchRequired (false, true, false),
         "new callsign band alone enters the callsign matcher");
  expect(AutoCallPolicy::logMatchRequired (false, false, true),
         "DXCC new band-mode alone enters the DXCC matcher");
  expect(!AutoCallPolicy::logMatchRequired (false, false, false),
         "disabled rare-target projections do not enter a matcher");
  expect(AutoCallPolicy::newGridNeedsReevaluation (true, true, true, false, 15),
         "new grid reevaluates while old DX call is retained");
  expect(AutoCallPolicy::newGridNeedsReevaluation (true, true, true, false, 16),
         "new grid LOTW variant reevaluates");
  expect(!AutoCallPolicy::newGridNeedsReevaluation (true, true, true, false, 13),
         "new grid does not reevaluate band/mode priority");
  expect(!AutoCallPolicy::newGridNeedsReevaluation (true, true, false, true, 15),
         "grid band/mode does not reevaluate new-grid priority");
  expect(AutoCallPolicy::newGridNeedsReevaluation (true, true, false, true, 13),
         "grid band/mode reevaluates its priority");
  expect(AutoCallPolicy::newGridNeedsReevaluation (true, true, true, true, 14),
         "both grid options include band/mode priority");
  expect(!AutoCallPolicy::newGridNeedsReevaluation (true, true, true, true, 17),
         "both grid options exclude unrelated priority");
  expect(!AutoCallPolicy::newGridNeedsReevaluation (false, true, true, true, 15),
         "new grid without retained DX uses ordinary candidate path");
  expect(!AutoCallPolicy::newGridNeedsReevaluation (true, false, true, true, 15),
         "old DX without a new grid does not retrigger AutoSeq");

  expect(AutoCallPolicy::isAnswerCQRetryStatus (AutoCallPolicy::rfinStatus, false, false),
         "RFIN is an answer-CQ retry status for automatic TX1");
  expect(AutoCallPolicy::isAnswerCQRetryStatus (AutoCallPolicy::rcqStatus, false, false),
         "RCQ keeps the existing answer-CQ retry status");
  expect(AutoCallPolicy::isAnswerCQRetryStatus (AutoCallPolicy::scallStatus, false, false),
         "SCALL keeps the existing answer-CQ retry status");
  expect(AutoCallPolicy::isAnswerCQRetryStatus (AutoCallPolicy::sreportStatus, true, false),
         "skipped-TX1 SREPORT keeps the existing answer-CQ retry status");
  expect(AutoCallPolicy::answerCQRetryLimitReached (AutoCallPolicy::rfinStatus, false, false,
                                                    true, 2, 2, false),
         "RFIN reaches the TX1 retry limit and must clear DX");
  expect(!AutoCallPolicy::answerCQRetryLimitReached (AutoCallPolicy::rfinStatus, false, false,
                                                     true, 2, 1, false),
         "RFIN remains active below the TX1 retry limit");
  expect(AutoCallPolicy::answerCQRetryLimitReached (AutoCallPolicy::rfinStatus, false, false,
                                                    true, 2, 1, true),
         "reply-to-other still terminates the RFIN retry");
  expect(!AutoCallPolicy::answerCQRetryLimitReached (AutoCallPolicy::rfinStatus, false, true,
                                                     true, 1, 1, false),
         "Hound mode keeps its separate retry path");
  expect(!AutoCallPolicy::isAnswerCQRetryStatus (AutoCallPolicy::sreportStatus, false, false),
         "SREPORT without skipped TX1 is not an answer-CQ retry");

  // 终止动作必须同时考虑回答 CQ 状态、计数开关、目标优先级和回复他台。
  // 每个自动起呼优先级组都用其真实配置项组合验证，不能只测独立 helper。
  const auto actionFor = [] (int status, int priority, bool autoTarget,
                             bool skipTx1, bool houndMode, bool counterEnabled,
                             int limit, int count, bool replyOther,
                             bool strictDirectionalCQ) {
    return AutoCallPolicy::answerCQRetryAction (
        status, skipTx1, houndMode, counterEnabled, limit, count, replyOther,
        autoTarget, priority, strictDirectionalCQ);
  };
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (22, true, false, false, false, false, false),
         "priority 22 maps to configured new DXCC");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (23, true, false, false, false, false, false),
         "priority 23 maps to configured new DXCC band variant");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (20, false, true, false, false, false, false),
         "priority 20 maps to configured DXCC band-mode");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (21, false, true, false, false, false, false),
         "priority 21 maps to configured DXCC band-mode variant");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (15, false, false, true, false, false, false),
         "priority 15 maps to configured new grid");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (16, false, false, true, false, false, false),
         "priority 16 maps to configured new grid LOTW variant");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (13, false, false, false, true, false, false),
         "priority 13 maps to configured grid band-mode");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (14, false, false, false, true, false, false),
         "priority 14 maps to configured grid band-mode LOTW variant");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (7, false, false, false, false, true, false),
         "priority 7 maps to configured new call");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (8, false, false, false, false, true, false),
         "priority 8 maps to configured new call LOTW variant");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (5, false, false, false, false, false, true),
         "priority 5 maps to configured new call band");
  expect(AutoCallPolicy::isConfiguredAutomaticTarget (6, false, false, false, false, false, true),
         "priority 6 maps to configured new call band LOTW variant");

  struct AutomaticTargetConfig {
    int priority;
    bool newDXCC;
    bool newDXCCBandMode;
    bool newGrid;
    bool newGridBandMode;
    bool newCall;
    bool newCallBand;
  };
  const AutomaticTargetConfig automaticTargets[] = {
      {22, true, false, false, false, false, false},
      {23, true, false, false, false, false, false},
      {20, false, true, false, false, false, false},
      {21, false, true, false, false, false, false},
      {15, false, false, true, false, false, false},
      {16, false, false, true, false, false, false},
      {13, false, false, false, true, false, false},
      {14, false, false, false, true, false, false},
      {7, false, false, false, false, true, false},
      {8, false, false, false, false, true, false},
      {5, false, false, false, false, false, true},
      {6, false, false, false, false, false, true}};
  for (auto const& target : automaticTargets) {
    bool const automaticTarget = AutoCallPolicy::isConfiguredAutomaticTarget (
        target.priority, target.newDXCC, target.newDXCCBandMode, target.newGrid,
        target.newGridBandMode, target.newCall, target.newCallBand);
    expect(automaticTarget, "configured automatic target reaches action selector");
    expect(actionFor (AutoCallPolicy::rcqStatus, target.priority, automaticTarget, false, false,
                      true, 2, 2, false, false)
               == AutoCallPolicy::AnswerCQRetryAction::standbyCleanup,
           "automatic target threshold selects standby cleanup for every priority group");
  }
  expect(actionFor (AutoCallPolicy::rfinStatus, 20, true, false, false,
                    true, 2, 2, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::standbyCleanup,
         "automatic RFIN threshold selects standby cleanup");
  expect(actionFor (AutoCallPolicy::scallStatus, 23, true, false, false,
                    true, 2, 1, true, false)
             == AutoCallPolicy::AnswerCQRetryAction::standbyCleanup,
         "automatic SCALL reply-other selects standby cleanup");
  expect(actionFor (AutoCallPolicy::sreportStatus, 15, true, true, false,
                    true, 2, 2, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::standbyCleanup,
         "automatic skipped-TX1 SREPORT selects standby cleanup");
  expect(actionFor (AutoCallPolicy::rcqStatus, 22, true, false, false,
                    true, 2, 1, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::none,
         "automatic target below threshold remains active");
  expect(actionFor (AutoCallPolicy::rcqStatus, 22, true, false, false,
                    false, 2, 2, true, false)
             == AutoCallPolicy::AnswerCQRetryAction::none,
         "disabled answer-CQ counter does not terminate automatic target");
  expect(actionFor (AutoCallPolicy::rcqStatus, 22, true, false, true,
                    true, 2, 2, true, false)
             == AutoCallPolicy::AnswerCQRetryAction::none,
         "Hound remains isolated from automatic target cleanup");
  expect(actionFor (AutoCallPolicy::sreportStatus, 15, true, false, false,
                    true, 2, 2, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::none,
         "SREPORT without skipped TX1 is not automatic answer-CQ cleanup");
  expect(actionFor (AutoCallPolicy::rcqStatus, 15, false, false, false,
                    true, 2, 2, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::legacyCleanup,
         "ordinary priority uses legacy cleanup");
  expect(actionFor (AutoCallPolicy::rcqStatus, 15, false, false, false,
                    true, 2, 1, true, false)
             == AutoCallPolicy::AnswerCQRetryAction::legacyCleanup,
         "ordinary reply-other uses legacy cleanup");
  expect(actionFor (AutoCallPolicy::rcqStatus, 22, false, false, false,
                    true, 2, 2, false, false)
             == AutoCallPolicy::AnswerCQRetryAction::none,
         "unconfigured high priority has no answer-CQ cleanup");
  expect(actionFor (AutoCallPolicy::rcqStatus, 22, false, false, false,
                    true, 2, 2, false, true)
             == AutoCallPolicy::AnswerCQRetryAction::legacyCleanup,
         "strict directional CQ keeps ordinary high-priority cleanup");

  expect(AutoCallPolicy::canForceCandidate (false, false, false, false, false),
         "cleared DX permits a fresh candidate to be selected and armed");

  expect(!AutoCallPolicy::canForceCandidate (true, false, false, false, false),
         "retained active QSO cannot be replaced");
  expect(!AutoCallPolicy::canForceCandidate (true, true, false, false, true),
         "completed AutoSeq interval cannot be replaced");
  expect(!AutoCallPolicy::canForceCandidate (true, false, true, false, true),
         "pending first 73 keeps the retained QSO");
  expect(!AutoCallPolicy::canForceCandidate (true, false, false, true, true),
         "transmitting keeps the retained QSO");
  expect(AutoCallPolicy::canForceCandidate (true, false, false, false, true),
         "Tx5-completed QSO may be replaced");

  return 0;
}
