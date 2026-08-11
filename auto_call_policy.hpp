#ifndef AUTO_CALL_POLICY_HPP__
#define AUTO_CALL_POLICY_HPP__

#include <QStringList>

namespace AutoCallPolicy
{
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
