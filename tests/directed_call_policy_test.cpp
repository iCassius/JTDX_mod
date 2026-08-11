#include "directed_call_policy.hpp"
#include "qsohistory.h"

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  QsoHistory::Status select (QsoHistory& history, unsigned options,
                             QString& call, QString& report)
  {
    QString grid;
    int rx = 0;
    int tx = 0;
    unsigned time = options;
    int count = 0;
    int priority = 0;
    QString mode;
    return history.autoseq (call, grid, report, rx, tx, time, count, priority, mode);
  }

  void addCall (QsoHistory& history, QString const& call,
                QsoHistory::Status status, unsigned time,
                int priority = 0, QString const& report = QString {})
  {
    history.message (call, status, priority, report, QString {}, QStringLiteral ("NA"),
                     QStringLiteral ("W"), time, report, 700, QStringLiteral ("FT8"));
  }
}

int main ()
{
  unsigned const directed = 128u | QsoHistory::AutoAnswerDirectedCalls
      | QsoHistory::AutoAnswerDirectedOnly;

  expect (!DirectedCallPolicy::canSelectStandbyCall (false, true, true, false, false, false, false, false),
          "disabled option does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, false, false, false, false, false, false),
          "active QSO does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, true, true, false, false, false, false),
          "Hound does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, true, false, true, false, false, false),
          "WSPR does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, true, false, false, true, false, false),
          "transmitting does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, true, false, false, false, true, false),
          "tune does not select");
  expect (!DirectedCallPolicy::canSelectStandbyCall (true, true, true, false, false, false, false, true),
          "PTT does not select");
  expect (DirectedCallPolicy::shouldArm (true, true, true, false, false, false, false, false,
                                         DirectedCallPolicy::rcallStatus),
          "RCALL arms");
  expect (DirectedCallPolicy::shouldArm (true, true, true, false, false, false, false, false,
                                         DirectedCallPolicy::rreportStatus),
          "RREPORT arms");
  expect (!DirectedCallPolicy::shouldArm (true, true, true, false, false, false, false, false, 12),
          "RRR does not arm");
  expect (DirectedCallPolicy::standardTxButtonForStatus (DirectedCallPolicy::rcallStatus) == 2,
          "RCALL maps to Tx2");
  expect (DirectedCallPolicy::standardTxButtonForStatus (DirectedCallPolicy::rreportStatus) == 3,
          "RREPORT maps to Tx3");
  expect (DirectedCallPolicy::standardTxButtonForStatus (12) == 0,
          "completion has no standby Tx mapping");

  expect (DirectedCallPolicy::isBetterCandidate (QStringLiteral ("K1AAA"), 0, 0, 0,
                                                 true, QStringLiteral ("W1ABC"), 0, 0, 0, false),
          "same-strategy tie breaks by callsign");
  expect (!DirectedCallPolicy::isBetterCandidate (QStringLiteral ("W1ABC"), 0, 0, 0,
                                                  true, QStringLiteral ("K1AAA"), 0, 0, 0, false),
          "tie-break is stable");

  QsoHistory multiple;
  multiple.init ();
  multiple.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"), QStringLiteral ("OM89"), false);
  multiple.time (100);
  addCall (multiple, QStringLiteral ("W1ABC"), QsoHistory::RCALL, 100);
  addCall (multiple, QStringLiteral ("K1AAA"), QsoHistory::RCALL, 100);
  QString call;
  QString report;
  expect (select (multiple, directed, call, report) == QsoHistory::RCALL,
          "multiple directed calls select RCALL");
  expect (call == QStringLiteral ("K1AAA"), "multiple calls use deterministic tie-break");

  QsoHistory reportCall;
  reportCall.init ();
  reportCall.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"), QStringLiteral ("OM89"), false);
  reportCall.time (200);
  addCall (reportCall, QStringLiteral ("W1ABC"), QsoHistory::RREPORT, 200, 0, QStringLiteral ("+05"));
  call.clear ();
  report.clear ();
  expect (select (reportCall, directed, call, report) == QsoHistory::RREPORT,
          "RREPORT is selected");
  expect (call == QStringLiteral ("W1ABC"), "RREPORT selects caller");

  QsoHistory cq;
  cq.init ();
  cq.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"), QStringLiteral ("OM89"), false);
  cq.time (300);
  addCall (cq, QStringLiteral ("W1ABC"), QsoHistory::RCQ, 300);
  call.clear ();
  expect (select (cq, directed, call, report) == QsoHistory::NONE,
          "ordinary CQ is not a directed standby entry point");

  QsoHistory completion;
  completion.init ();
  completion.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"), QStringLiteral ("OM89"), false);
  completion.time (400);
  addCall (completion, QStringLiteral ("W1ABC"), QsoHistory::RREPORT, 399, 0, QStringLiteral ("+05"));
  addCall (completion, QStringLiteral ("W1ABC"), QsoHistory::SREPORT, 399, 0, QStringLiteral ("+05"));
  addCall (completion, QStringLiteral ("W1ABC"), QsoHistory::RRR, 400, 0, QStringLiteral ("+05"));
  call.clear ();
  expect (select (completion, directed, call, report) == QsoHistory::NONE,
          "isolated completion is not a new standby QSO");

  QsoHistory queue;
  queue.init ();
  queue.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"), QStringLiteral ("OM89"), false);
  queue.time (500);
  addCall (queue, QStringLiteral ("W1ABC"), QsoHistory::RCALL, 500);
  call.clear ();
  expect (select (queue, directed, call, report) == QsoHistory::RCALL,
          "new decode selects directed call");
  queue.time (501);
  call.clear ();
  expect (select (queue, directed, call, report) == QsoHistory::NONE,
          "old directed decode is not queued for delayed reply");
  addCall (queue, QStringLiteral ("W1ABC"), QsoHistory::RCALL, 501);
  call.clear ();
  expect (select (queue, directed, call, report) == QsoHistory::RCALL,
          "new directed decode can retrigger after stop");

  return 0;
}
