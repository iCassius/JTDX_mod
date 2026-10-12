#include <QString>
#include <QtMath>
#include "ptt_start_policy.hpp"
#include <array>
#include <cstdio>
#include <cstring>

int g_iptt = 0;
int volatile icw[1] {};

struct FakeClock
{
  qint64 milliseconds {0};
  qint64 currentMSecsSinceEpoch2 () const { return milliseconds; }
};

class SlotControlHarness
{
public:
  double m_TRperiod {15.0};
  QString m_modeTx {QStringLiteral ("FT8")};
  QString m_mode {QStringLiteral ("FT8")};
  bool m_txFirst {false};
  bool m_bTxTime {false};
  bool m_tune {false};
  bool m_enableTx {false};
  bool m_transmitting {false};
  bool m_tx_when_ready {false};
  bool m_btxok {false};
  bool m_haltTrans {false};
  bool m_tuneStopPending {false};
  double m_nseq {0.0};
  FakeClock clock;
  FakeClock *m_jtdxtime {&clock};
  int stopCount {0};
  struct PttStopFlags
  {
    bool enable_tx;
    bool btxok;
    bool transmitting;
    bool tune;
    bool tx_when_ready;
    int g_iptt;
  };
  int stopDiagnosticAttempts {0};
  QString lastStopDiagnosticEvent;
  QString lastStopDiagnosticDetail;
  int lastStopDiagnosticValue {-1};
  PttStopFlags lastStopDiagnosticBefore {};
  PttStopFlags lastStopDiagnosticAfter {};

  void stopTx ()
  {
    ++stopCount;
    m_btxok = false;
    m_transmitting = false;
    g_iptt = 0;
  }

  void appendPttStopDiagnostic (char const * event,
                                PttStopFlags const& before,
                                PttStopFlags const& after,
                                char const * detail_name,
                                int detail_value) noexcept
  {
    ++stopDiagnosticAttempts;
    try
      {
        lastStopDiagnosticEvent = QString::fromLatin1 (event);
        lastStopDiagnosticDetail = QString::fromLatin1 (detail_name);
        lastStopDiagnosticValue = detail_value;
        lastStopDiagnosticBefore = before;
        lastStopDiagnosticAfter = after;
      }
    catch (...)
      {
        // Match the production diagnostic's no-throw boundary.
      }
  }

  void update038 (bool btxok0)
  {
    double txDuration;
#include "production_slot_current.inc"
#include "p0_tx_slot_scope_038.inc"
  }

  void updateCurrent (bool btxok0)
  {
    double txDuration;
#include "production_slot_current.inc"
#include "production_scope_current.inc"
  }
};

namespace
{
enum class FailureKind { Other, RxBoundaryStop, LaterSlotTxResidue };

struct FailureCounts
{
  int other {0};
  int rxBoundaryStop {0};
  int laterSlotTxResidue {0};
};

FailureCounts counts;

void check (bool condition, FailureKind kind, char const * message)
{
  if (condition) return;
  std::fprintf (stderr, "FAIL: %s\n", message);
  switch (kind)
    {
    case FailureKind::Other: ++counts.other; break;
    case FailureKind::RxBoundaryStop: ++counts.rxBoundaryStop; break;
    case FailureKind::LaterSlotTxResidue: ++counts.laterSlotTxResidue; break;
    }
}

struct Phase
{
  bool txFirst;
  qint64 start;
  qint64 rx;
  qint64 next;
  char const * name;
};

constexpr std::array<Phase, 4> phases {{
  {true, 100, 14000, 30100, "0/30 first"},
  {true, 30100, 44000, 60100, "0/30 second"},
  {false, 15100, 29000, 45100, "15/45 first"},
  {false, 45100, 59000, 75100, "15/45 second"}
}};

template<class Update>
void boundary (Phase phase, Update update, bool expect_slot_end_diagnostic = false)
{
  SlotControlHarness harness;
  harness.m_txFirst = phase.txFirst;
  harness.m_transmitting = true;
  harness.m_btxok = true;
  harness.clock.milliseconds = phase.start;
  g_iptt = 1;
  update (harness, true);
  check (harness.m_bTxTime, FailureKind::Other,
         "FT8 TX phase is active before RX boundary");
  check (harness.m_btxok && harness.m_transmitting && g_iptt == 1
         && harness.stopCount == 0, FailureKind::Other,
         "active TX retained before RX boundary");

  harness.clock.milliseconds = phase.rx;
  update (harness, harness.m_btxok);
  check (!harness.m_bTxTime, FailureKind::Other,
         "FT8 RX phase is active at boundary");
  check (harness.stopCount == 1 && !harness.m_transmitting && g_iptt == 0
         && !harness.m_btxok, FailureKind::RxBoundaryStop,
         phase.name);
  if (expect_slot_end_diagnostic)
    check (harness.stopDiagnosticAttempts == 1
           && harness.lastStopDiagnosticEvent == QStringLiteral ("tx_slot_end")
           && harness.lastStopDiagnosticDetail == QStringLiteral ("previous_btxok")
           && harness.lastStopDiagnosticValue == 1
           // The production slot path clears btxok before sampling stop flags.
           && !harness.lastStopDiagnosticBefore.btxok
           && harness.lastStopDiagnosticBefore.transmitting
           && harness.lastStopDiagnosticBefore.g_iptt == 1
           && !harness.lastStopDiagnosticAfter.btxok
           && !harness.lastStopDiagnosticAfter.transmitting
           && harness.lastStopDiagnosticAfter.g_iptt == 0,
           FailureKind::Other,
           "slot-end diagnostic records the actual production stop edge and state transition");

  for (int slot = 0; slot < 2; ++slot)
    {
      harness.m_enableTx = false;
      harness.clock.milliseconds = phase.next + 30000LL * slot;
      update (harness, harness.m_btxok);
      check (harness.m_bTxTime, FailureKind::Other,
             "later FT8 slot was calculated");
      check (!harness.m_transmitting && g_iptt == 0 && harness.stopCount == 1,
             FailureKind::LaterSlotTxResidue,
             "no implicit TX recovery while Enable TX is off");
      if (expect_slot_end_diagnostic)
        check (harness.stopDiagnosticAttempts == 1,
               FailureKind::Other,
               "later receive slots do not repeat the one-shot slot-end diagnostic");
    }
}

template<class Update>
void tune (Update update)
{
  SlotControlHarness harness;
  harness.m_transmitting = true;
  harness.m_btxok = true;
  harness.m_tune = true;
  g_iptt = 1;
  for (qint64 milliseconds : {14000LL, 29000LL, 44000LL, 59000LL})
    {
      harness.clock.milliseconds = milliseconds;
      update (harness, true);
      check (harness.m_bTxTime && harness.m_btxok && harness.m_transmitting
             && g_iptt == 1 && harness.stopCount == 0,
             FailureKind::Other, "Tune remains active across slot boundary");
    }
}

void run038 ()
{
  for (Phase phase : phases)
    boundary (phase, [] (SlotControlHarness& harness, bool btxok0)
      { harness.update038 (btxok0); });
  tune ([] (SlotControlHarness& harness, bool btxok0)
    { harness.update038 (btxok0); });
}

void runCurrent ()
{
  for (Phase phase : phases)
    boundary (phase, [] (SlotControlHarness& harness, bool btxok0)
      { harness.updateCurrent (btxok0); }, true);
  tune ([] (SlotControlHarness& harness, bool btxok0)
    { harness.updateCurrent (btxok0); });

  SlotControlHarness halt_only;
  halt_only.m_haltTrans = true;
  g_iptt = 0;
  halt_only.updateCurrent (false);
  check (halt_only.stopCount == 1 && !halt_only.m_haltTrans
         && halt_only.stopDiagnosticAttempts == 0,
         FailureKind::Other,
         "non-slot halt still stops but is not mislabeled as a slot-end edge");
}

void printCounts (char const * profile)
{
  std::printf ("profile=%s other=%d rx_boundary_stop=%d later_slot_tx_residue=%d\n",
               profile, counts.other, counts.rxBoundaryStop,
               counts.laterSlotTxResidue);
}

bool exact038Defect ()
{
  return counts.other == 0 && counts.rxBoundaryStop == 4
      && counts.laterSlotTxResidue == 8;
}
}

int main (int argc, char ** argv)
{
  if (argc != 2) return 2;
  if (std::strcmp (argv[1], "--baseline-038") == 0)
    {
      run038 ();
      printCounts (argv[1]);
      if (exact038Defect ())
        {
          std::fprintf (stderr,
                        "expected 038 defect reproduced: 4 RX stop misses, 8 later-slot TX residue assertions\n");
          return 1;
        }
      std::fprintf (stderr, "038 direct negative run did not match expected defect signature\n");
      return 3;
    }
  if (std::strcmp (argv[1], "--verify-baseline-038") == 0)
    {
      run038 ();
      printCounts (argv[1]);
      if (!exact038Defect ())
        {
          std::fprintf (stderr, "038 defect signature mismatch\n");
          return 1;
        }
      std::printf ("verified exact 038 defect signature\n");
      return 0;
    }
  if (std::strcmp (argv[1], "--current") == 0)
    {
      runCurrent ();
      printCounts (argv[1]);
      if (counts.other || counts.rxBoundaryStop || counts.laterSlotTxResidue)
        return 1;
      std::puts ("current production control flow passed");
      return 0;
    }
  return 2;
}
