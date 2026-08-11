#ifndef DIRECTED_CALL_POLICY_HPP__
#define DIRECTED_CALL_POLICY_HPP__

#include <QString>

// Keep the unattended directed-call decision independent from MainWindow so
// the safety boundary and standard-message mapping can be tested without
// starting Qt widgets, CAT, PTT, audio, or a decoder process.
namespace DirectedCallPolicy
{
  // QsoHistory::Status values are intentionally kept here as integer values
  // to avoid a header dependency cycle.  RCALL and RREPORT are the only
  // valid standby entry points for this feature.
  constexpr int rcallStatus = 4;
  constexpr int rreportStatus = 6;

  inline bool isDirectedCallStatus (int status)
  {
    return status == rcallStatus || status == rreportStatus;
  }

  inline int standardTxButtonForStatus (int status)
  {
    if (status == rcallStatus) return 2;
    if (status == rreportStatus) return 3;
    return 0;
  }

  inline bool canSelectStandbyCall (bool enabled, bool autoSeq,
                                    bool noActiveQso, bool hound,
                                    bool wspr, bool transmitting, bool tune,
                                    bool iptt)
  {
    return enabled && autoSeq && noActiveQso && !hound && !wspr
        && !transmitting && !tune && !iptt;
  }

  inline bool shouldArm (bool enabled, bool autoSeq, bool noActiveQso,
                         bool hound, bool wspr, bool transmitting, bool tune,
                         bool iptt, int status)
  {
    return canSelectStandbyCall (enabled, autoSeq, noActiveQso, hound, wspr,
                                 transmitting, tune, iptt)
        && isDirectedCallStatus (status);
  }

  inline bool preferDirectedCandidate (bool candidateIsDirected,
                                       bool selectedIsDirected)
  {
    return candidateIsDirected && !selectedIsDirected;
  }

  inline bool isBetterCandidate (QString const& candidateCall,
                                 int candidatePriority, int candidateReport,
                                 int candidateDistance, bool selectedExists,
                                 QString const& selectedCall,
                                 int selectedPriority, int selectedReport,
                                 int selectedDistance, bool maxDistance)
  {
    if (!selectedExists || candidatePriority > selectedPriority) return true;
    if (candidatePriority != selectedPriority) return false;
    auto const candidateMetric = maxDistance ? candidateDistance : candidateReport;
    auto const selectedMetric = maxDistance ? selectedDistance : selectedReport;
    if (candidateMetric != selectedMetric) return candidateMetric > selectedMetric;
    return candidateCall < selectedCall;
  }
}

#endif
