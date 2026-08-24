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
