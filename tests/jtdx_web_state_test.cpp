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
  check (json.value (QStringLiteral ("rig_generation")).isNull (), "empty rig generation is null");
  check (!json.contains (QStringLiteral ("freshness"))
             && !json.contains (QStringLiteral ("generated_at"))
             && !json.contains (QStringLiteral ("stale_after_ms")),
         "snapshot omits user-facing freshness and age fields");
  state.set_cycle_clock_provider ([] { return qint64 {123456789}; }, [] { return 7.5; });
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("jtdx_time_ms")).toVariant ().toLongLong () == 123456789
             && json.value (QStringLiteral ("cycle_period_ms")).toInt () == 7500,
         "snapshot exposes the JTDX corrected clock and actual mode cycle period");
  check (json.value (QStringLiteral ("mode")).isNull (), "empty mode is null");
  check (json.value (QStringLiteral ("frequency")).isNull (), "empty CAT frequency is null");
  check (json.value (QStringLiteral ("recent_decodes")).isArray (), "decodes is an array");
  check (json.value (QStringLiteral ("auto_sequence_state")).isNull (), "empty AutoSeq state is null");
  check (json.value (QStringLiteral ("frequency_candidates")).toArray ().isEmpty (),
         "empty frequency candidate list");
  check (JtdxWebState::project_cq_state (false, true, true) == QStringLiteral ("not_selected"), "non-CQ TX is not CQ state");
  check (JtdxWebState::project_cq_state (true, false, false) == QStringLiteral ("idle"), "selected CQ idle state");
  check (JtdxWebState::project_cq_state (true, true, false) == QStringLiteral ("armed"), "selected CQ armed state");
  check (JtdxWebState::project_cq_state (true, true, true) == QStringLiteral ("transmitting"), "selected CQ transmitting state");

  state.observe_status (14074000u, QStringLiteral ("FT8"), QString {}, QStringLiteral ("-10"),
                        QStringLiteral ("FT8"), true, false, false, 1200, 1300,
                        QStringLiteral ("N0CALL"), QStringLiteral ("FN31"), QString {},
                        false, QString {}, false, false);
  check (state.rig_generation () == 0, "status observation does not create rig generation");
  json = state.json_snapshot ();
  check (!json.contains (QStringLiteral ("freshness")), "status does not add a freshness display field");
  check (json.value (QStringLiteral ("target_frequency")).toDouble () == 14074000, "target frequency");
  check (json.value (QStringLiteral ("frequency")).isNull (), "target is not CAT frequency");
  check (json.value (QStringLiteral ("tx_enabled")).isBool (), "boolean JSON type");
  state.observe_decode (true, QTime {12, 34, 56}, -10, 0.1F, 1500,
                        QStringLiteral ("FT8"), QStringLiteral ("K1ABC FN31"), false, false,
                        QStringLiteral ("K1ABC"), QStringLiteral ("FN31"),
                        QStringLiteral ("United States"), QStringLiteral ("California"),
                        QStringLiteral ("NA"));
  auto decode_json = state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ().first ().toObject ();
  check (decode_json.value (QStringLiteral ("country")).toString () == QStringLiteral ("United States")
             && decode_json.value (QStringLiteral ("entity")).toString () == QStringLiteral ("United States")
             && decode_json.value (QStringLiteral ("province")).toString () == QStringLiteral ("California")
             && decode_json.value (QStringLiteral ("continent")).toString () == QStringLiteral ("NA"),
         "decode geography publishes entity, supported province, and continent separately");
  JtdxWebState::DecodeSelection selection;
  check (state.decode_selection (1, &selection) && selection.call == QStringLiteral ("K1ABC")
             && selection.grid == QStringLiteral ("FN31") && selection.delta_frequency == 1500,
         "fresh realtime decode can be selected by stable id");
  auto const dx_generation_before = state.dx_generation ();
  state.observe_web_dx_selection (selection.call, selection.grid, QStringLiteral ("decode"),
                                  selection.decode_id, selection.delta_frequency, selection.time);
  check (state.dx_generation () == dx_generation_before + 1,
         "DX selection has an independent generation");
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("dx_selection_source")).toString () == QStringLiteral ("decode")
             && json.value (QStringLiteral ("dx_source_decode_id")).toInt () == 1,
         "DX selection source metadata is published");
  state.observe_business_state (true, QStringLiteral ("calling"), QStringLiteral ("armed"), QStringLiteral ("CQ N0CALL FN31"));
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("auto_sequence_state")).toString () == QStringLiteral ("enabled"), "AutoSeq state");
  check (json.value (QStringLiteral ("qso_stage")).toString () == QStringLiteral ("calling"), "QSO stage");
  check (json.value (QStringLiteral ("cq_state")).toString () == QStringLiteral ("armed"), "CQ state");
  check (json.value (QStringLiteral ("current_tx_text")).toString () == QStringLiteral ("CQ N0CALL FN31"), "current TX text");
  JtdxWebState::FrequencyCandidates candidates {
    {14074000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("All"), false},
    {14074000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("All"), true},
    {7074000u, QStringLiteral ("40m"), QStringLiteral ("FT8"), QStringLiteral ("Region 1"), true},
    {0u, QStringLiteral ("invalid"), QStringLiteral ("FT8"), QStringLiteral ("All"), true}
  };
  state.set_frequency_candidates (QStringLiteral ("FT8"), QStringLiteral ("Region 1"), candidates);
  json = state.json_snapshot ();
  auto const candidate_array = json.value (QStringLiteral ("frequency_candidates")).toArray ();
  check (candidate_array.size () == 2, "frequency candidates reject zero and duplicate Hz rows");
  check (json.value (QStringLiteral ("frequency_candidate_mode")).toString () == QStringLiteral ("FT8"),
         "frequency candidate mode context");
  check (json.value (QStringLiteral ("frequency_candidate_region")).toString () == QStringLiteral ("Region 1"),
         "frequency candidate region context");
  check (candidate_array.first ().toObject ().value (QStringLiteral ("frequency_hz")).toString ()
             == QStringLiteral ("14074000"), "frequency candidate Hz is an exact string");
  check (candidate_array.first ().toObject ().value (QStringLiteral ("default")).toBool (),
         "duplicate candidate metadata is retained");
  auto const candidate_revision = state.revision ();
  state.set_frequency_candidates (QStringLiteral ("FT8"), QStringLiteral ("Region 1"), candidates);
  check (state.revision () == candidate_revision, "unchanged frequency candidates do not churn revision");
  state.set_frequency_candidates (QStringLiteral ("JT9"), QStringLiteral ("Region 2"), {});
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("frequency_candidates")).toArray ().isEmpty (),
         "mode or region changes can publish an empty candidate list");
  check (json.value (QStringLiteral ("frequency_candidate_mode")).toString () == QStringLiteral ("JT9"),
         "empty candidate list keeps the new mode context");
  auto revision_after_status = state.revision ();
  state.advance_clock_for_test (5001);
  check (!state.json_snapshot ().contains (QStringLiteral ("freshness")), "elapsed time does not add a freshness display field");

  state.observe_rig (true, 14074123u, 14074150u, false);
  check (state.rig_generation () == 1, "first rig event increments rig generation");
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("frequency")).toDouble () == 14074123, "CAT reported frequency");
  check (json.value (QStringLiteral ("target_frequency")).toDouble () == 14074000, "CAT and target remain distinct");
  check (state.revision () > revision_after_status, "revision increments");
  state.observe_rig (false, 0, 0, false);
  check (state.rig_generation () == 2, "offline rig event is still a new rig observation");
  json = state.json_snapshot ();
  check (json.value (QStringLiteral ("rig_online")).toBool (true) == false, "offline CAT invalidation");
  check (json.value (QStringLiteral ("frequency")).isNull (), "offline CAT frequency unknown");
  check (json.value (QStringLiteral ("ptt")).isNull (), "offline CAT PTT unknown");
  state.observe_rig (true, 14074123u, 14074150u, false);
  auto const rig_generation_after_observations = state.rig_generation ();

  state.set_decode_limit (999);
  check (state.decode_limit () == JtdxWebState::hard_decode_limit, "hard decode limit");
  for (int i = 0; i < 501; ++i)
    state.observe_decode (true, QTime {0, 0, 1}, -10, 0.1f, static_cast<quint32> (i),
                          QStringLiteral ("FT8"), QStringLiteral ("CQ TEST"), false, false,
                          QStringLiteral ("N0CALL"), QStringLiteral ("FN31"));
  check (state.rig_generation () == rig_generation_after_observations,
         "decode observations do not create rig generation");
  json = state.json_snapshot ();
  auto decodes = json.value (QStringLiteral ("recent_decodes")).toArray ();
  check (decodes.size () == JtdxWebState::hard_decode_limit, "decode hard cap");
  check (decodes.first ().toObject ().value (QStringLiteral ("decode_id")).toDouble () == 3, "oldest decode evicted");
  check (decodes.last ().toObject ().value (QStringLiteral ("callsign")).toString () == QStringLiteral ("N0CALL"), "decoded callsign is carried through");
  check (decodes.last ().toObject ().value (QStringLiteral ("grid")).toString () == QStringLiteral ("FN31"), "decoded grid is carried through");
  auto const live_decode_id = decodes.last ().toObject ().value (QStringLiteral ("decode_id")).toDouble ();
  check (!decodes.last ().toObject ().contains (QStringLiteral ("fresh"))
             && !decodes.last ().toObject ().contains (QStringLiteral ("age_ms")),
         "decode rows omit freshness and age fields");
  state.advance_clock_for_test (5001);

  int const before_replay_count = decodes.size ();
  state.observe_decode (false, QTime {0, 0, 2}, -5, 0.2f, 1, QStringLiteral ("FT8"),
                        QStringLiteral ("replay"), false, false);
  check (!state.json_snapshot ().contains (QStringLiteral ("last_decode_update")),
         "decode replay does not expose a last-update clock");
  check (state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ().size () == before_replay_count,
         "replay does not grow decode buffer");
  state.observe_decode (true, QTime {0, 0, 3}, -5, 0.2f, 1, QStringLiteral ("FT8"),
                        QStringLiteral ("off-air"), false, true);
  check (!state.json_snapshot ().contains (QStringLiteral ("last_decode_update"))
             && !state.json_snapshot ().contains (QStringLiteral ("rig_fresh")),
         "off-air and elapsed observations do not create age fields");
  check (state.decode_selection (static_cast<quint64> (live_decode_id), &selection),
         "selection of a retained live decode has no Web snapshot-age timeout gate");

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
