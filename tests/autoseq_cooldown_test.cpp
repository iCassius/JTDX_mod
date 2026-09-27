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

  void setup (QsoHistory& history, unsigned cqTime = 900)
  {
    history.init ();
    history.owndata (QStringLiteral ("AS"), QStringLiteral ("BI"),
                     QStringLiteral ("OM89"), false);
    history.message (QStringLiteral ("CQ"), QsoHistory::SCQ, 0, QString {},
                     QString {}, QString {}, QString {}, cqTime, QString {},
                     700, QStringLiteral ("FT8"));
  }

  void addCandidate (QsoHistory& history, QString const& call,
                     QsoHistory::Status status, int priority, unsigned time,
                     QString const& report)
  {
    history.message (call, status, priority, QStringLiteral ("FN31"),
                     QString {}, QStringLiteral ("NA"), QStringLiteral ("W"),
                     time, report, 700, QStringLiteral ("FT8"));
  }

  void addReply (QsoHistory& history, QString const& call,
                 QsoHistory::Status status, unsigned time)
  {
    history.message (call, status, 17, QStringLiteral ("-10"), QString {},
                     QStringLiteral ("NA"), QStringLiteral ("W"), time,
                     QStringLiteral ("-10"), 700, QStringLiteral ("FT8"));
  }

  QsoHistory::Status select (QsoHistory& history, unsigned options = 0)
  {
    QString call;
    QString grid;
    QString report;
    QString mode;
    int rx = 0;
    int tx = 0;
    int count = 0;
    int priority = 0;
    unsigned time = options;
    return history.autoseq (call, grid, report, rx, tx, time, count, priority,
                            mode);
  }

  QsoHistory::Status selectMixedPath (bool specialFailure,
                                     QsoHistory::Status candidateStatus,
                                     unsigned options = 0,
                                     QString const& report = QStringLiteral ("-10"))
  {
    QsoHistory history;
    setup (history);
    int const priority = options & QsoHistory::AutoCallNewDXCC ? 22 : 17;
    addCandidate (history, QStringLiteral ("W1ABC"), candidateStatus,
                  priority, 1000, report);
    history.time (1000);
    history.calllist (QStringLiteral ("W1ABC"), -10, 1000, specialFailure);
    return select (history, options);
  }
}

int main ()
{
  QsoHistory snapshot;
  setup (snapshot);
  addCandidate (snapshot, QStringLiteral ("W1ABC"), QsoHistory::RCQ, 17,
                1000, QStringLiteral ("-10"));
  QsoHistory::Status snapshotStatus = QsoHistory::NONE;
  int snapshotCount = -1;
  require (snapshot.diagnosticSnapshot (QStringLiteral ("W1ABC"),
                                        snapshotStatus, snapshotCount)
               && snapshotStatus == QsoHistory::RCQ && snapshotCount == 0,
           "diagnostic snapshot reads the current QSO status and retry count");
  for (unsigned time : {1016u, 1032u})
    snapshot.message (QStringLiteral ("W1ABC"), QsoHistory::SCALL, 17,
                      QString {}, QString {}, QStringLiteral ("NA"),
                      QStringLiteral ("W"), time, QStringLiteral ("-10"),
                      700, QStringLiteral ("FT8"));
  require (snapshot.diagnosticSnapshot (QStringLiteral ("W1ABC"),
                                        snapshotStatus, snapshotCount)
               && snapshotStatus == QsoHistory::SCALL && snapshotCount == 2,
           "diagnostic snapshot reports accumulated outgoing attempts");
  snapshotStatus = QsoHistory::FIN;
  snapshotCount = 99;
  require (!snapshot.diagnosticSnapshot (QStringLiteral ("K1MISSING"),
                                         snapshotStatus, snapshotCount)
               && snapshotStatus == QsoHistory::FIN && snapshotCount == 99,
           "missing snapshot leaves output values unchanged");

  // The current-batch mixed candidate path must honor a special-target
  // failure even with rare-target options disabled, without broadening the
  // old no-rare behavior for ordinary calllist records.
  for (auto status : {QsoHistory::RCQ, QsoHistory::RFIN})
    {
      require (selectMixedPath (true, status) == QsoHistory::NONE,
               "special RCQ/RFIN failure is cooled with rare flags off");
      require (selectMixedPath (false, status) == status,
               "ordinary calllist does not add cooldown with rare flags off");
      require (selectMixedPath (false, status, QsoHistory::AutoCallNewDXCC)
                   == QsoHistory::NONE,
               "rare flags retain the existing calllist cooldown");
    }

  // The recent-CQ mixed path scans a previous decode while a different
  // current-batch candidate keeps AutoSeq active.
  auto recent = [] (bool specialFailure, unsigned options = 0) {
    QsoHistory history;
    setup (history, 900);
    int const priority = options & QsoHistory::AutoCallNewDXCC ? 22 : 17;
    addCandidate (history, QStringLiteral ("W1ABC"), QsoHistory::RCQ, priority,
                  1000, QStringLiteral ("-10"));
    history.calllist (QStringLiteral ("W1ABC"), -10, 1000, specialFailure);
    addCandidate (history, QStringLiteral ("K1AAA"), QsoHistory::RCQ, 0,
                  1016, QStringLiteral ("-10"));
    history.time (1016);
    return select (history, options);
  };
  require (recent (true) == QsoHistory::NONE,
           "special failure is cooled in the recent mixed path");
  require (recent (false) == QsoHistory::RCQ,
           "ordinary record keeps the legacy no-rare recent-path behavior");
  require (recent (false, QsoHistory::AutoCallNewDXCC) == QsoHistory::NONE,
           "rare flags retain general calllist cooldown in the recent path");

  // The third, CQ-only candidate path has always applied ordinary calllist
  // cooldown. Verify special failures still reach that guard.
  auto cqOnly = [] (bool recordFailure, bool specialFailure) {
    QsoHistory history;
    setup (history);
    addCandidate (history, QStringLiteral ("W1ABC"), QsoHistory::RCQ, 15,
                  1000, QStringLiteral ("-10"));
    if (recordFailure)
      history.calllist (QStringLiteral ("W1ABC"), -10, 1000, specialFailure);
    history.time (1000);
    return select (history, 1u);
  };
  require (cqOnly (true, true) == QsoHistory::NONE,
           "special failure is cooled in the CQ-only path");
  require (cqOnly (true, false) == QsoHistory::NONE,
           "ordinary calllist cooldown remains in the CQ-only path");
  require (cqOnly (false, false) == QsoHistory::RCQ,
           "CQ-only path admits a candidate without a failure record");

  // Every real reply stage from the same station must remain selectable
  // despite the special CQ failure record.
  for (auto status : {QsoHistory::RCALL, QsoHistory::RREPORT,
                      QsoHistory::RRREPORT, QsoHistory::RRR,
                      QsoHistory::RRR73})
    {
      QsoHistory reply;
      setup (reply);
      addCandidate (reply, QStringLiteral ("W1ABC"), QsoHistory::RCQ, 17,
                    1000, QStringLiteral ("-10"));
      reply.calllist (QStringLiteral ("W1ABC"), -10, 1000, true);
      if (status == QsoHistory::RCALL || status == QsoHistory::RREPORT)
        addReply (reply, QStringLiteral ("W1ABC"), status, 1016);
      else
        {
          reply.message (QStringLiteral ("W1ABC"), QsoHistory::SREPORT, 17,
                         QStringLiteral ("-10"), QStringLiteral ("S"),
                         QStringLiteral ("NA"), QStringLiteral ("W"), 1016,
                         QStringLiteral ("-10"), 700, QStringLiteral ("FT8"));
          addReply (reply, QStringLiteral ("W1ABC"), QsoHistory::RREPORT, 1032);
          if (status == QsoHistory::RRR || status == QsoHistory::RRR73)
            addReply (reply, QStringLiteral ("W1ABC"), status, 1048);
        }
      unsigned const currentTime = status == QsoHistory::RCALL || status == QsoHistory::RREPORT
          ? 1016u : (status == QsoHistory::RRREPORT ? 1032u : 1048u);
      reply.time (currentTime);
      require (select (reply) == status,
               "real QSO response bypasses the special CQ cooldown");
    }

  // Reports, the exact 300-second bound, stronger/weaker reports, and
  // midnight wrap are exercised through QsoHistory::autoseq itself.
  auto candidateAt = [] (unsigned failedAt, unsigned candidateAt,
                         QString const& report) {
    QsoHistory history;
    setup (history, failedAt == 86300 ? 86200 : 900);
    addCandidate (history, QStringLiteral ("W1ABC"), QsoHistory::RCQ, 17,
                  candidateAt, report);
    history.calllist (QStringLiteral ("W1ABC"), -10, failedAt, true);
    history.time (candidateAt);
    return select (history);
  };
  require (candidateAt (1000, 1300, QStringLiteral ("-10")) == QsoHistory::NONE,
           "same report remains cooled at exactly 300 seconds");
  require (candidateAt (1000, 1301, QStringLiteral ("-10")) == QsoHistory::RCQ,
           "same report re-enters after 300 seconds");
  require (candidateAt (1000, 1100, QStringLiteral ("-15")) == QsoHistory::NONE,
           "weaker report remains cooled");
  require (candidateAt (1000, 1100, QStringLiteral ("-5")) == QsoHistory::RCQ,
           "stronger report competes before cooldown expiry");
  require (candidateAt (1000, 1100, QStringLiteral ("invalid")) == QsoHistory::NONE,
           "invalid candidate report maps to -60 and remains cooled");
  require (candidateAt (1000, 1100, QString {}) == QsoHistory::NONE,
           "empty candidate report maps to -60 and remains cooled");
  require (candidateAt (86300, 100, QStringLiteral ("-10")) == QsoHistory::NONE,
           "cooldown crosses midnight");
  require (candidateAt (86300, 201, QStringLiteral ("-10")) == QsoHistory::RCQ,
           "midnight cooldown releases after 300 seconds");

  return 0;
}
