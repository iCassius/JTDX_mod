#include "JtdxWebState.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include <cstdint>

namespace {
int failures = 0;

void check (bool condition, char const * message)
{
  if (!condition)
    {
      qCritical () << message;
      ++failures;
    }
}
}

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  JtdxWebState state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("instance")};
  JtdxWebState generated_id {QStringLiteral ("JTDX"), QStringLiteral ("test")};
  check (!generated_id.json_snapshot ().value (QStringLiteral ("instance_id")).isNull (), "generated instance id");
  state.set_clock_for_test (100);

  auto json = state.json_snapshot ();
  check (json.value (QStringLiteral ("freshness")).toString () == QStringLiteral ("unknown"), "empty state freshness");
  check (json.value (QStringLiteral ("mode")).isNull (), "empty mode is null");
  check (json.value (QStringLiteral ("frequency")).isNull (), "empty CAT frequency is null");
  check (json.value (QStringLiteral ("recent_decodes")).isArray (), "decodes is an array");
  check (json.value (QStringLiteral ("auto_sequence_state")).isNull (), "empty AutoSeq state is null");
  check (JtdxWebState::project_cq_state (false, true, true) == QStringLiteral ("not_selected"), "non-CQ TX is not CQ state");
  check (JtdxWebState::project_cq_state (true, false, false) == QStringLiteral ("idle"), "selected CQ idle state");
  check (JtdxWebState::project_cq_state (true, true, false) == QStringLiteral ("armed"), "selected CQ armed state");
  check (JtdxWebState::project_cq_state (true, true, true) == QStringLiteral ("transmitting"), "selected CQ transmitting state");

  state.observe_status (14074000u, QStringLiteral ("FT8"), QString {}, QStringLiteral ("-10"),
                        QStringLiteral ("FT8"), true, false, false, 1200, 1300,
                        QStringLiteral ("N0CALL"), QStringLiteral ("FN31"), QString {},
                        false, QString {}, false, false);
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("freshness")).toString () == QStringLiteral ("fresh"), "fresh status");
  check (json.value (QStringLiteral ("target_frequency")).toDouble () == 14074000, "target frequency");
  check (json.value (QStringLiteral ("frequency")).isNull (), "target is not CAT frequency");
  check (json.value (QStringLiteral ("tx_enabled")).isBool (), "boolean JSON type");
  state.observe_business_state (true, QStringLiteral ("calling"), QStringLiteral ("armed"), QStringLiteral ("CQ N0CALL FN31"));
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("auto_sequence_state")).toString () == QStringLiteral ("enabled"), "AutoSeq state");
  check (json.value (QStringLiteral ("qso_stage")).toString () == QStringLiteral ("calling"), "QSO stage");
  check (json.value (QStringLiteral ("cq_state")).toString () == QStringLiteral ("armed"), "CQ state");
  check (json.value (QStringLiteral ("current_tx_text")).toString () == QStringLiteral ("CQ N0CALL FN31"), "current TX text");
  auto revision_after_status = state.revision ();
  state.advance_clock_for_test (5001);
  check (state.json_snapshot ().value (QStringLiteral ("freshness")).toString () == QStringLiteral ("stale"), "stale status");

  state.observe_rig (true, 14074123u, 14074150u, false);
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("frequency")).toDouble () == 14074123, "CAT reported frequency");
  check (json.value (QStringLiteral ("target_frequency")).toDouble () == 14074000, "CAT and target remain distinct");
  check (state.revision () > revision_after_status, "revision increments");
  state.observe_rig (false, 0, 0, false);
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("rig_online")).toBool (true) == false, "offline CAT invalidation");
  check (json.value (QStringLiteral ("frequency")).isNull (), "offline CAT frequency unknown");
  check (json.value (QStringLiteral ("ptt")).isNull (), "offline CAT PTT unknown");
  state.observe_rig (true, 14074123u, 14074150u, false);

  state.set_decode_limit (999);
  check (state.decode_limit () == JtdxWebState::hard_decode_limit, "hard decode limit");
  for (int i = 0; i < 501; ++i)
    state.observe_decode (true, QTime {0, 0, 1}, -10, 0.1f, static_cast<quint32> (i),
                          QStringLiteral ("FT8"), QStringLiteral ("CQ TEST"), false, false,
                          QStringLiteral ("N0CALL"), QStringLiteral ("FN31"));
  json = state.json_snapshot ();
  auto decodes = json.value (QStringLiteral ("recent_decodes")).toArray ();
  check (decodes.size () == JtdxWebState::hard_decode_limit, "decode hard cap");
  check (decodes.first ().toObject ().value (QStringLiteral ("decode_id")).toDouble () == 2, "oldest decode evicted");
  check (decodes.last ().toObject ().value (QStringLiteral ("callsign")).toString () == QStringLiteral ("N0CALL"), "decoded callsign is carried through");
  check (decodes.last ().toObject ().value (QStringLiteral ("grid")).toString () == QStringLiteral ("FN31"), "decoded grid is carried through");
  auto const live_decode_id = decodes.last ().toObject ().value (QStringLiteral ("decode_id")).toDouble ();
  check (decodes.last ().toObject ().value (QStringLiteral ("fresh")).toBool (), "live decode starts fresh");
  state.advance_clock_for_test (5001);

  auto before_replay = json.value (QStringLiteral ("last_decode_update"));
  int const before_replay_count = decodes.size ();
  state.observe_decode (false, QTime {0, 0, 2}, -5, 0.2f, 1, QStringLiteral ("FT8"),
                        QStringLiteral ("replay"), false, false);
  check (state.json_snapshot ().value (QStringLiteral ("last_decode_update")) == before_replay,
         "replay does not refresh decode freshness");
  check (state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ().size () == before_replay_count,
         "replay does not grow decode buffer");
  state.observe_decode (true, QTime {0, 0, 3}, -5, 0.2f, 1, QStringLiteral ("FT8"),
                        QStringLiteral ("off-air"), false, true);
  check (state.json_snapshot ().value (QStringLiteral ("last_decode_update")) == before_replay,
         "off-air does not refresh decode freshness");
  check (!state.json_snapshot ().value (QStringLiteral ("rig_fresh")).toBool (), "CAT state becomes stale");
  bool live_decode_is_stale = false;
  for (auto const& value : state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ())
    {
      auto const item = value.toObject ();
      if (item.value (QStringLiteral ("decode_id")).toDouble () == live_decode_id)
        live_decode_is_stale = !item.value (QStringLiteral ("fresh")).toBool ();
    }
  check (live_decode_is_stale, "the same live decode becomes stale");

  auto before_clear = state.revision ();
  state.clear_decodes ();
  json = state.json_snapshot ();
  check (state.revision () > before_clear, "clear increments revision");
  check (json.value (QStringLiteral ("recent_decodes")).toArray ().isEmpty (), "clear removes decodes");
  state.observe_wspr_decode (true, QTime {0, 0, 4}, -12, 0.3f, 136000000u, 2,
                            QStringLiteral ("W1AW"), QStringLiteral ("FN31"), 30, false);
  auto wspr = state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ().first ().toObject ();
  check (wspr.value (QStringLiteral ("frequency")).toDouble () == 136000000, "WSPR frequency type/value");
  check (wspr.value (QStringLiteral ("callsign")).isString (), "WSPR callsign JSON type");
  state.clear_decodes ();
  state.observe_wspr_decode (true, QTime {0, 0, 5}, -11, 0.2f,
                             static_cast<JtdxWebState::Frequency> (static_cast<quint64> (UINT32_MAX) + 1u),
                             0, QStringLiteral ("W1AW"), QStringLiteral ("FN31"), 20, false);
  auto wide_wspr = state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ().first ().toObject ();
  check (wide_wspr.value (QStringLiteral ("frequency")).toDouble () == static_cast<double> (static_cast<quint64> (UINT32_MAX) + 1u), "WSPR frequency exceeds UINT32");
  return failures == 0 ? 0 : 1;
}
