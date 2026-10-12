#include "cat_health_policy.hpp"

#include <QFile>
#include <QString>

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace
{
  void check (bool condition, char const * expression, int line)
  {
    if (!condition)
      {
        std::cerr << "CHECK failed at line " << line << ": " << expression << "\n";
        std::exit (1);
      }
  }

#define CHECK(expression) check (static_cast<bool> (expression), #expression, __LINE__)

  bool production_reports_successful_ftx1_ptt_get ()
  {
    QFile source {QStringLiteral (JTDX_SOURCE_DIR "/HamlibTransceiver.cpp")};
    if (!source.open (QIODevice::ReadOnly)) return false;
    auto text = QString::fromUtf8 (source.readAll ());
    text.replace (QStringLiteral ("\r\n"), QStringLiteral ("\n"));
    auto const poll = text.indexOf (QStringLiteral ("void HamlibTransceiver::do_poll ()"));
    auto const get = text.indexOf (
      QStringLiteral ("auto const rc = rig_get_ptt"), poll);
    auto const guard = text.indexOf (
      QStringLiteral ("has_new_protocol_sync_events"), get);
    auto const success = text.indexOf (QStringLiteral ("if (RIG_OK == rc)"), guard);
    auto const confirm = text.indexOf (
      QStringLiteral ("confirm_PTT_readback (ptt_on);"), success);
    auto const report = text.indexOf (
      QStringLiteral ("report_poll_read_result (QStringLiteral (\"rig_get_ptt\"), rc, true);"),
      confirm);
    auto const next_error_check = text.indexOf (
      QStringLiteral ("if (RIG_OK != rc)"), report);
    return get >= 0 && guard > get && success > guard && confirm > success
      && report > confirm && next_error_check > report;
  }

  constexpr std::int64_t ms (std::int64_t value) { return value * 1000000LL; }
  constexpr std::int64_t sec (std::int64_t value) { return value * 1000000000LL; }
}

int main ()
{
  CHECK(production_reports_successful_ftx1_ptt_get ());
  CatHealthPolicy policy;
  policy.begin_session (7, true, true, true, sec (1));
  CHECK(policy.evaluate (sec (1)).state == CatHealthPolicy::State::unknown);
  CHECK(!policy.poll_started (6, 1, sec (2), 1000));       // stale generation
  CHECK(!policy.poll_started (7, 1, sec (0), 1000));       // before runtime fence
  CHECK(policy.poll_started (7, 1, sec (2), 1000));
  CHECK(policy.poll_read (7, 1, sec (2) + ms (100), true, true,
                            12, 31, 0));
  CHECK(policy.poll_completed (7, 1, sec (2), sec (2) + ms (150),
                                 1000, false));
  auto state = policy.evaluate (sec (2) + ms (151));
  CHECK(state.state == CatHealthPolicy::State::healthy);
  CHECK(state.reason == CatHealthPolicy::Reason::valid_get);
  CHECK(state.control_sequence == 12 && state.ptt_readback_serial == 31);

  // An unchanged state is still healthy when the passive poll entered,
  // completed, and returned a valid GET.  The observer does not require a
  // control-state change to count the read.
  CHECK(policy.poll_started (7, 2, sec (3), 1000));
  CHECK(policy.poll_read (7, 2, sec (3) + ms (30), true, true,
                            12, 32, 0));
  CHECK(policy.poll_completed (7, 2, sec (3), sec (3) + ms (50),
                                 1000, false));
  CHECK(policy.evaluate (sec (3) + ms (51)).state
          == CatHealthPolicy::State::healthy);

  // Failed API reads and protocol-invalid replies are distinct inputs, but
  // both invalidate the health observation without issuing recovery.
  CHECK(policy.poll_started (7, 3, sec (4), 1000));
  CHECK(policy.poll_read (7, 3, sec (4) + ms (10), false, true,
                            12, 32, -11));
  state = policy.evaluate (sec (4) + ms (11));
  CHECK(state.state == CatHealthPolicy::State::suspect);
  CHECK(state.reason == CatHealthPolicy::Reason::invalid_get);
  CHECK(policy.poll_completed (7, 3, sec (4), sec (4) + ms (20),
                                 1000, false));
  CHECK(policy.evaluate (sec (4) + ms (21)).state
          == CatHealthPolicy::State::suspect);

  CHECK(policy.poll_started (7, 4, sec (5), 1000));
  CHECK(policy.poll_read (7, 4, sec (5) + ms (10), true, false,
                            12, 32, 0));
  CHECK(policy.poll_completed (7, 4, sec (5), sec (5) + ms (20),
                                 1000, false));
  CHECK(policy.evaluate (sec (5) + ms (21)).state
          == CatHealthPolicy::State::suspect);

  // A valid, explicit refresh can renew the PTT GET age for a previously
  // observed runtime session, but without any poll event the label is unknown.
  CHECK(policy.refresh_read (7, sec (6), true, true, true, 12, 33, 0));
  state = policy.evaluate (sec (6) + ms (1));
  CHECK(state.state == CatHealthPolicy::State::healthy);
  CHECK(state.ptt_readback_serial == 33);
  CHECK(policy.refresh_read (7, sec (7), true, false, true, 12, 33, -5));
  CHECK(policy.evaluate (sec (7) + ms (1)).state
          == CatHealthPolicy::State::suspect);
  CHECK(!policy.refresh_read (7, sec (8), false, true, true, 12, 34, 0));
  CHECK(policy.evaluate (sec (8)).state == CatHealthPolicy::State::suspect);

  CatHealthPolicy same_stamp_failure;
  same_stamp_failure.begin_session (70, true, true, true, sec (1));
  CHECK(same_stamp_failure.poll_started (70, 1, sec (2), 1000));
  CHECK(same_stamp_failure.poll_read (70, 1, sec (2) + ms (1), true, true,
                                       1, 1, 0));
  CHECK(same_stamp_failure.poll_completed (70, 1, sec (2), sec (2) + ms (1),
                                             1000, false));
  CHECK(same_stamp_failure.refresh_read (70, sec (2) + ms (1), true,
                                          false, true, 1, 1, -1));
  CHECK(same_stamp_failure.evaluate (sec (2) + ms (2)).state
          == CatHealthPolicy::State::suspect);

  CatHealthPolicy manual_refresh_without_poll;
  manual_refresh_without_poll.begin_session (71, true, true, true, sec (1));
  CHECK(manual_refresh_without_poll.refresh_read (71, sec (2), true,
                                                    true, true, 1, 1, 0));
  CHECK(manual_refresh_without_poll.evaluate (sec (2)).state
          == CatHealthPolicy::State::unknown);

  // Five seconds is a separate observational slow-call threshold.  It does
  // not depend on a potentially long legal polling cadence.
  policy.begin_session (8, true, true, true, sec (10));
  CHECK(policy.poll_started (8, 1, sec (10), 999000));
  CHECK(policy.poll_read (8, 1, sec (10) + ms (10), true, true,
                            20, 40, 0));
  CHECK(policy.poll_completed (8, 1, sec (10), sec (10) + ms (20),
                                 999000, false));
  CHECK(policy.evaluate (sec (510)).state == CatHealthPolicy::State::healthy);
  CHECK(policy.evaluate (sec (1012) + ms (100)).state
          == CatHealthPolicy::State::suspect); // 999s cadence + 3s grace

  policy.begin_session (9, true, true, true, sec (2000));
  CHECK(policy.poll_started (9, 1, sec (2000), 500));
  state = policy.evaluate (sec (2005) + ms (1));
  CHECK(state.state == CatHealthPolicy::State::suspect);
  CHECK(state.reason == CatHealthPolicy::Reason::poll_stalled);
  CHECK(policy.poll_read (9, 1, sec (2006), true, true, 1, 1, 0));
  CHECK(policy.poll_completed (9, 1, sec (2000), sec (2006) + ms (1),
                                 500, false));
  state = policy.evaluate (sec (2006) + ms (2));
  CHECK(state.state == CatHealthPolicy::State::suspect);
  CHECK(state.reason == CatHealthPolicy::Reason::poll_slow);

  // Configuration-test, inactive, and unsupported sessions never claim
  // health, and stale-generation callbacks cannot revive a later session.
  policy.begin_session (10, false, true, true, sec (3000));
  CHECK(!policy.poll_started (10, 1, sec (3001), 1000));
  CHECK(policy.evaluate (sec (3001)).state == CatHealthPolicy::State::unknown);
  policy.begin_session (11, true, false, true, sec (4000));
  CHECK(!policy.poll_started (11, 1, sec (4001), 1000));
  policy.begin_session (12, true, true, false, sec (5000));
  CHECK(!policy.poll_started (12, 1, sec (5001), 1000));
  policy.begin_session (13, true, true, true, sec (6000));
  CHECK(!policy.poll_started (12, 9, sec (6001), 1000));
  CHECK(policy.evaluate (sec (6001)).state == CatHealthPolicy::State::unknown);

  policy.begin_session (14, true, true, true, sec (7000));
  CHECK(policy.should_log (sec (7000)));
  CHECK(!policy.should_log (sec (7000) + sec (29)));
  CHECK(policy.should_log (sec (7000) + sec (30)));
  CHECK(policy.poll_started (14, 1, sec (7031), 500));
  policy.evaluate (sec (7037));
  CHECK(policy.should_log (sec (7037))); // transition unknown -> suspect

  std::cout << "cat_health_policy_test: PASS\n";
  return 0;
}
