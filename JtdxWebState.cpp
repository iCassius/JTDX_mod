#include "JtdxWebState.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QMetaType>
#include <QUuid>
#include <QtGlobal>
#include <utility>

namespace {
constexpr qint64 stale_after_ms = 5000;
}

constexpr int JtdxWebState::default_decode_limit;
constexpr int JtdxWebState::hard_decode_limit;
constexpr int JtdxWebState::default_frequency_candidate_limit;
constexpr int JtdxWebState::hard_frequency_candidate_limit;

QString JtdxWebState::project_cq_state (bool cq_selected, bool enable_tx, bool transmitting)
{
  if (!cq_selected) return QStringLiteral ("not_selected");
  if (transmitting) return QStringLiteral ("transmitting");
  return enable_tx ? QStringLiteral ("armed") : QStringLiteral ("idle");
}

JtdxWebState::JtdxWebState (QString application_name, QString application_version,
                            QString instance_id, QObject * parent)
  : QObject {parent}
  , application_name_ {std::move (application_name)}
  , application_version_ {std::move (application_version)}
  , instance_id_ {instance_id.isEmpty () ? QUuid::createUuid ().toString (QUuid::WithoutBraces)
                                         : std::move (instance_id)}
{
  clock_.start ();
}

void JtdxWebState::set_decode_limit (int limit)
{
  decode_limit_ = qBound (0, limit, hard_decode_limit);
  trim_decodes ();
  bump_revision ();
}

void JtdxWebState::observe_status (Frequency target_frequency, QString const& mode,
                                   QString const& dx_call, QString const& report,
                                   QString const& tx_mode, bool tx_enabled, bool transmitting,
                                   bool decoding, qint32 rx_df, qint32 tx_df,
                                   QString const& de_call, QString const& de_grid,
                                   QString const& dx_grid, bool watchdog_timeout,
                                   QString const& sub_mode, bool fast_mode, bool tx_first,
                                   bool /*force*/)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  ++revision_;
  has_status_ = true;
  status_seen_ms_ = monotonic_now ();
  status_wall_ = QDateTime::currentDateTimeUtc ();
  target_frequency_ = target_frequency;
  has_target_frequency_ = target_frequency != 0;
  mode_ = mode;
  dx_call_ = dx_call;
  report_ = report;
  tx_mode_ = tx_mode;
  tx_enabled_ = tx_enabled;
  transmitting_ = transmitting;
  decoding_ = decoding;
  rx_df_ = rx_df;
  tx_df_ = tx_df;
  de_call_ = de_call;
  de_grid_ = de_grid;
  dx_grid_ = dx_grid;
  watchdog_timeout_ = watchdog_timeout;
  sub_mode_ = sub_mode;
  fast_mode_ = fast_mode;
  tx_first_ = tx_first;
}

void JtdxWebState::observe_rig (bool online, Frequency reported_frequency,
                                Frequency reported_tx_frequency, bool ptt)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  ++revision_;
  ++rig_generation_;
  has_rig_ = true;
  rig_online_ = online;
  rig_frequency_ = reported_frequency;
  rig_tx_frequency_ = reported_tx_frequency;
  rig_ptt_ = ptt;
  rig_seen_ms_ = monotonic_now ();
  rig_wall_ = QDateTime::currentDateTimeUtc ();
}

void JtdxWebState::observe_band (QString const& band)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  if (band_ == band) return;
  ++revision_;
  band_ = band;
}

void JtdxWebState::observe_decode (bool is_new, QTime time, qint32 snr,
                                   float delta_time, quint32 delta_frequency,
                                   QString const& mode, QString const& message,
                                   bool low_confidence, bool off_air,
                                   QString const& callsign, QString const& grid,
                                   QString const& country)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  // replayDecodes 明确使用 is_new=false；回放数据已在 UI 流中，不能挤占实时条目。
  if (!is_new) return;
  ++revision_;
  qint64 const received_ms = monotonic_now ();
  Decode decode;
  decode.id = next_decode_id_++;
  decode.time = time.toString (Qt::ISODate);
  decode.snr = snr;
  decode.delta_time = delta_time;
  decode.delta_frequency = delta_frequency;
  decode.mode = mode;
  decode.message = message;
  decode.callsign = callsign;
  decode.grid = grid;
  decode.country = country;
  decode.low_confidence = low_confidence;
  decode.off_air = off_air;
  decode.is_new = is_new;
  decode.source_revision = revision_;
  decode.received_ms = received_ms;
  decodes_.append (decode);
  trim_decodes ();
  // 回放和离线数据仍可展示，但不能刷新实时流的新鲜度或成为实时候选。
  if (is_new && !off_air)
    {
      decode_seen_ms_ = monotonic_now ();
      decode_wall_ = QDateTime::currentDateTimeUtc ();
    }
}

void JtdxWebState::observe_wspr_decode (bool is_new, QTime time, qint32 snr,
                                        float delta_time, Frequency frequency,
                                        qint32 drift, QString const& callsign,
                                        QString const& grid, qint32 power, bool off_air)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  if (!is_new) return;
  ++revision_;
  Decode decode;
  decode.id = next_decode_id_++;
  decode.time = time.toString (Qt::ISODate);
  decode.snr = snr;
  decode.delta_time = delta_time;
  decode.mode = QStringLiteral ("WSPR");
  decode.low_confidence = false;
  decode.off_air = off_air;
  decode.is_new = is_new;
  decode.source_revision = revision_;
  decode.received_ms = monotonic_now ();
  decode.wspr = true;
  decode.frequency = frequency;
  decode.drift = drift;
  decode.callsign = callsign;
  decode.grid = grid;
  decode.power = power;
  decodes_.append (decode);
  trim_decodes ();
  if (!off_air)
    {
      decode_seen_ms_ = decode.received_ms;
      decode_wall_ = QDateTime::currentDateTimeUtc ();
    }
}

void JtdxWebState::observe_business_state (bool auto_sequence_enabled,
                                           QString const& qso_stage,
                                           QString const& cq_state,
                                           QString const& current_tx_text)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  bool const changed = !has_business_state_
      || auto_sequence_enabled_ != auto_sequence_enabled
      || qso_stage_ != qso_stage || cq_state_ != cq_state
      || current_tx_text_ != current_tx_text;
  ++revision_;
  if (changed) ++business_generation_;
  has_business_state_ = true;
  auto_sequence_enabled_ = auto_sequence_enabled;
  qso_stage_ = qso_stage;
  cq_state_ = cq_state;
  current_tx_text_ = current_tx_text;
}

void JtdxWebState::observe_radio_controls (bool multi_decode, bool agc_compensation,
                                           bool narrow, bool sync, bool skip_tx1,
                                           int current_tx_index, QStringList const& tx_messages,
                                           bool can_log_qso, QJsonObject const& qso_draft,
                                           quint64 qso_generation)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  QStringList bounded_messages;
  for (int index = 0; index < 6; ++index)
    bounded_messages.append (index < tx_messages.size () ? tx_messages.at (index).left (64)
                                                           : QString {});
  current_tx_index = qBound (0, current_tx_index, 6);
  bool const changed = !has_radio_controls_
      || multi_decode_ != multi_decode || agc_compensation_ != agc_compensation
      || narrow_ != narrow || sync_ != sync || skip_tx1_ != skip_tx1
      || current_tx_index_ != current_tx_index || tx_messages_ != bounded_messages
      || can_log_qso_ != can_log_qso || qso_draft_ != qso_draft
      || qso_generation_ != qso_generation;
  if (changed) ++revision_;
  has_radio_controls_ = true;
  multi_decode_ = multi_decode;
  agc_compensation_ = agc_compensation;
  narrow_ = narrow;
  sync_ = sync;
  skip_tx1_ = skip_tx1;
  current_tx_index_ = current_tx_index;
  tx_messages_ = std::move (bounded_messages);
  can_log_qso_ = can_log_qso;
  qso_draft_ = qso_draft;
  qso_generation_ = qso_generation;
}

bool JtdxWebState::decode_selection (quint64 decode_id, DecodeSelection * selection) const
{
  if (!selection || decode_id == 0) return false;
  qint64 const now = monotonic_now ();
  constexpr qint64 stale_after_ms = 5000;
  for (auto const& decode : decodes_)
    if (decode.id == decode_id && decode.is_new && !decode.off_air
        && now - decode.received_ms <= stale_after_ms && !decode.callsign.isEmpty ())
      {
        selection->decode_id = decode.id;
        selection->source_revision = decode.source_revision;
        selection->time = decode.time;
        selection->delta_frequency = static_cast<qint32> (decode.delta_frequency);
        selection->call = decode.callsign;
        selection->grid = decode.grid;
        return true;
      }
  return false;
}

void JtdxWebState::observe_web_dx_selection (QString const& call, QString const& grid,
                                             QString const& source, quint64 source_decode_id,
                                             qint32 delta_frequency, QString const& time)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  ++revision_;
  ++dx_generation_;
  dx_call_ = call;
  dx_grid_ = grid;
  dx_selection_source_ = source;
  dx_source_decode_id_ = source_decode_id;
  dx_frequency_offset_ = delta_frequency;
  dx_time_ = time;
}

void JtdxWebState::set_frequency_candidates (QString const& mode, QString const& region,
                                              FrequencyCandidates const& candidates)
{
  Q_ASSERT (QThread::currentThread () == thread ());
  FrequencyCandidates normalized;
  QSet<QString> seen_frequencies;
  for (auto const& candidate : candidates)
    {
      if (candidate.frequency_hz == 0 || candidate.band.isEmpty ()) continue;
      QString const key = QString::number (static_cast<qulonglong> (candidate.frequency_hz));
      if (seen_frequencies.contains (key))
        {
          for (auto& existing : normalized)
            if (QString::number (static_cast<qulonglong> (existing.frequency_hz)) == key)
              {
                existing.default_frequency = existing.default_frequency || candidate.default_frequency;
                if (existing.band.isEmpty ()) existing.band = candidate.band;
                if (existing.mode.isEmpty ()) existing.mode = candidate.mode;
                if (existing.region.isEmpty ()) existing.region = candidate.region;
                break;
              }
          continue;
        }
      seen_frequencies.insert (key);
      normalized.append (candidate);
      if (normalized.size () >= hard_frequency_candidate_limit) break;
    }

  bool changed = frequency_candidate_mode_ != mode
    || frequency_candidate_region_ != region
    || frequency_candidates_.size () != normalized.size ();
  if (!changed)
    {
      for (int index = 0; index < normalized.size (); ++index)
        {
          auto const& lhs = frequency_candidates_.at (index);
          auto const& rhs = normalized.at (index);
          if (lhs.frequency_hz != rhs.frequency_hz || lhs.band != rhs.band
              || lhs.mode != rhs.mode || lhs.region != rhs.region
              || lhs.default_frequency != rhs.default_frequency)
            {
              changed = true;
              break;
            }
        }
    }
  if (!changed) return;

  frequency_candidate_mode_ = mode;
  frequency_candidate_region_ = region;
  frequency_candidates_ = std::move (normalized);
  bump_revision ();
}

void JtdxWebState::clear_decodes ()
{
  Q_ASSERT (QThread::currentThread () == thread ());
  ++revision_;
  decodes_.clear ();
}

void JtdxWebState::set_clock_for_test (qint64 monotonic_ms)
{
  test_clock_ = true;
  test_now_ms_ = qMax<qint64> (0, monotonic_ms);
}

void JtdxWebState::advance_clock_for_test (qint64 elapsed_ms)
{
  if (!test_clock_) set_clock_for_test (0);
  test_now_ms_ = qMax<qint64> (0, test_now_ms_ + elapsed_ms);
}

qint64 JtdxWebState::monotonic_now () const
{
  return test_clock_ ? test_now_ms_ : clock_.elapsed ();
}

void JtdxWebState::bump_revision ()
{
  ++revision_;
}

void JtdxWebState::trim_decodes ()
{
  while (decodes_.size () > decode_limit_)
    decodes_.removeFirst ();
}

QJsonValue JtdxWebState::nullable_string (QString const& value)
{
  return value.isEmpty () ? QJsonValue {QJsonValue::Null} : QJsonValue {value};
}

QJsonValue JtdxWebState::nullable_frequency (Frequency value, bool known)
{
  return known && value != 0 ? QJsonValue {static_cast<qint64> (value)}
                             : QJsonValue {QJsonValue::Null};
}

QJsonValue JtdxWebState::nullable_bool (bool value, bool known)
{
  return known ? QJsonValue {value} : QJsonValue {QJsonValue::Null};
}

QJsonObject JtdxWebState::json_snapshot () const
{
  Q_ASSERT (QThread::currentThread () == thread ());
  QJsonObject object;
  object.insert (QStringLiteral ("schema_version"), 1);
  object.insert (QStringLiteral ("application_name"), nullable_string (application_name_));
  object.insert (QStringLiteral ("application_version"), nullable_string (application_version_));
  object.insert (QStringLiteral ("instance_id"), nullable_string (instance_id_));
  object.insert (QStringLiteral ("server_epoch"), QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("state_revision"), static_cast<qint64> (revision_));
  object.insert (QStringLiteral ("revision"), static_cast<qint64> (revision_));
  object.insert (QStringLiteral ("generated_at"), QDateTime::currentDateTimeUtc ().toString (Qt::ISODateWithMs));
  object.insert (QStringLiteral ("stale_after_ms"), stale_after_ms);

  qint64 const now = monotonic_now ();
  QString freshness = QStringLiteral ("unknown");
  if (has_status_)
    freshness = now - status_seen_ms_ <= stale_after_ms ? QStringLiteral ("fresh")
                                                        : QStringLiteral ("stale");
  object.insert (QStringLiteral ("freshness"), freshness);
  object.insert (QStringLiteral ("online"), nullable_bool (has_status_, has_status_));
  object.insert (QStringLiteral ("rig_online"), nullable_bool (rig_online_, has_rig_));
  object.insert (QStringLiteral ("last_seen"), status_wall_.isValid ()
                ? QJsonValue {status_wall_.toString (Qt::ISODateWithMs)} : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("last_status_update"), status_wall_.isValid ()
                ? QJsonValue {status_wall_.toString (Qt::ISODateWithMs)} : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("last_decode_update"), decode_wall_.isValid ()
                ? QJsonValue {decode_wall_.toString (Qt::ISODateWithMs)} : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("target_frequency"), nullable_frequency (target_frequency_, has_target_frequency_));
  object.insert (QStringLiteral ("nominal_frequency"), nullable_frequency (target_frequency_, has_target_frequency_));
  object.insert (QStringLiteral ("frequency"), nullable_frequency (rig_frequency_, has_rig_ && rig_online_));
  object.insert (QStringLiteral ("rig_reported_frequency"), nullable_frequency (rig_frequency_, has_rig_ && rig_online_));
  object.insert (QStringLiteral ("rig_reported_tx_frequency"), nullable_frequency (rig_tx_frequency_, has_rig_ && rig_online_));
  object.insert (QStringLiteral ("rig_generation"), has_rig_ ? QJsonValue {static_cast<qint64> (rig_generation_)}
                                                               : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("rig_age_ms"), has_rig_ ? QJsonValue {qMax<qint64> (0, now - rig_seen_ms_)}
                                                           : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("rig_fresh"), nullable_bool (rig_online_ && has_rig_
                                                               && now - rig_seen_ms_ <= stale_after_ms,
                                                               has_rig_));
  object.insert (QStringLiteral ("mode"), nullable_string (mode_));
  object.insert (QStringLiteral ("band"), nullable_string (band_));
  object.insert (QStringLiteral ("frequency_candidate_mode"), nullable_string (frequency_candidate_mode_));
  object.insert (QStringLiteral ("frequency_candidate_region"), nullable_string (frequency_candidate_region_));
  object.insert (QStringLiteral ("frequency_candidate_limit"), hard_frequency_candidate_limit);
  object.insert (QStringLiteral ("dx_call"), nullable_string (dx_call_));
  object.insert (QStringLiteral ("dx_grid"), nullable_string (dx_grid_));
  object.insert (QStringLiteral ("dx_generation"), static_cast<qint64> (dx_generation_));
  object.insert (QStringLiteral ("dx_selection_source"), nullable_string (dx_selection_source_));
  object.insert (QStringLiteral ("dx_source_decode_id"), dx_source_decode_id_ == 0
                ? QJsonValue {QJsonValue::Null} : QJsonValue {static_cast<qint64> (dx_source_decode_id_)});
  object.insert (QStringLiteral ("dx_frequency_offset"), dx_frequency_offset_);
  object.insert (QStringLiteral ("dx_time"), nullable_string (dx_time_));
  object.insert (QStringLiteral ("report"), nullable_string (report_));
  object.insert (QStringLiteral ("tx_mode"), nullable_string (tx_mode_));
  object.insert (QStringLiteral ("tx_enabled"), nullable_bool (tx_enabled_, has_status_));
  object.insert (QStringLiteral ("transmitting"), nullable_bool (transmitting_, has_status_));
  object.insert (QStringLiteral ("decoding"), nullable_bool (decoding_, has_status_));
  object.insert (QStringLiteral ("rx_df"), has_status_ ? QJsonValue {rx_df_} : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("tx_df"), has_status_ ? QJsonValue {tx_df_} : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("de_call"), nullable_string (de_call_));
  object.insert (QStringLiteral ("de_grid"), nullable_string (de_grid_));
  object.insert (QStringLiteral ("watchdog_timeout"), nullable_bool (watchdog_timeout_, has_status_));
  object.insert (QStringLiteral ("sub_mode"), nullable_string (sub_mode_));
  object.insert (QStringLiteral ("fast_mode"), nullable_bool (fast_mode_, has_status_));
  object.insert (QStringLiteral ("tx_first"), nullable_bool (tx_first_, has_status_));
  object.insert (QStringLiteral ("ptt"), nullable_bool (rig_ptt_, has_rig_ && rig_online_
                                                          && now - rig_seen_ms_ <= stale_after_ms));
  object.insert (QStringLiteral ("web_server_state"), QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("auto_sequence_state"), has_business_state_
                ? QJsonValue {auto_sequence_enabled_ ? QStringLiteral ("enabled")
                                                      : QStringLiteral ("disabled")}
                : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("business_generation"), static_cast<qint64> (business_generation_));
  object.insert (QStringLiteral ("qso_stage"), has_business_state_ ? nullable_string (qso_stage_)
                                                                      : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("cq_state"), has_business_state_ ? nullable_string (cq_state_)
                                                                     : QJsonValue {QJsonValue::Null});
  object.insert (QStringLiteral ("current_tx_text"), has_business_state_
                ? nullable_string (current_tx_text_) : QJsonValue {QJsonValue::Null});
  QJsonObject radio_controls;
  radio_controls.insert (QStringLiteral ("known"), has_radio_controls_);
  radio_controls.insert (QStringLiteral ("multi_decode"), has_radio_controls_ ? QJsonValue {multi_decode_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("agc_compensation"), has_radio_controls_ ? QJsonValue {agc_compensation_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("narrow"), has_radio_controls_ ? QJsonValue {narrow_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("sync"), has_radio_controls_ ? QJsonValue {sync_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("skip_tx1"), has_radio_controls_ ? QJsonValue {skip_tx1_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("current_tx_index"), has_radio_controls_ ? QJsonValue {current_tx_index_} : QJsonValue {QJsonValue::Null});
  QJsonArray tx_messages;
  if (has_radio_controls_)
    for (auto const& message : tx_messages_) tx_messages.append (message);
  radio_controls.insert (QStringLiteral ("tx_messages"), tx_messages);
  radio_controls.insert (QStringLiteral ("can_log_qso"), has_radio_controls_ ? QJsonValue {can_log_qso_} : QJsonValue {QJsonValue::Null});
  radio_controls.insert (QStringLiteral ("qso_draft_open"), has_radio_controls_ && !qso_draft_.isEmpty ());
  radio_controls.insert (QStringLiteral ("qso_draft"), qso_draft_);
  radio_controls.insert (QStringLiteral ("qso_generation"), static_cast<qint64> (qso_generation_));
  object.insert (QStringLiteral ("radio_controls"), radio_controls);

  QJsonArray frequency_candidates;
  for (auto const& candidate : frequency_candidates_)
    {
      QJsonObject item;
      item.insert (QStringLiteral ("frequency_hz"),
                   QString::number (static_cast<qulonglong> (candidate.frequency_hz)));
      item.insert (QStringLiteral ("band"), candidate.band);
      item.insert (QStringLiteral ("mode"), candidate.mode);
      item.insert (QStringLiteral ("region"), candidate.region);
      item.insert (QStringLiteral ("default"), candidate.default_frequency);
      frequency_candidates.append (item);
    }
  object.insert (QStringLiteral ("frequency_candidates"), frequency_candidates);

  QJsonArray decode_array;
  for (auto const& decode : decodes_)
    {
      QJsonObject item;
      item.insert (QStringLiteral ("decode_id"), static_cast<qint64> (decode.id));
      item.insert (QStringLiteral ("time"), nullable_string (decode.time));
      item.insert (QStringLiteral ("snr"), decode.snr);
      item.insert (QStringLiteral ("delta_time"), decode.delta_time);
      item.insert (QStringLiteral ("delta_frequency"), static_cast<qint64> (decode.delta_frequency));
      item.insert (QStringLiteral ("mode"), nullable_string (decode.mode));
      item.insert (QStringLiteral ("message"), nullable_string (decode.message));
      item.insert (QStringLiteral ("callsign"), nullable_string (decode.callsign));
      item.insert (QStringLiteral ("grid"), nullable_string (decode.grid));
      item.insert (QStringLiteral ("country"), nullable_string (decode.country));
      item.insert (QStringLiteral ("low_confidence"), decode.low_confidence);
      item.insert (QStringLiteral ("off_air"), decode.off_air);
      item.insert (QStringLiteral ("is_new"), decode.is_new);
      item.insert (QStringLiteral ("realtime"), decode.is_new && !decode.off_air);
      item.insert (QStringLiteral ("fresh"), decode.is_new && !decode.off_air
                   && now - decode.received_ms <= stale_after_ms);
      item.insert (QStringLiteral ("age_ms"), qMax<qint64> (0, now - decode.received_ms));
      item.insert (QStringLiteral ("frequency"), nullable_frequency (decode.frequency, decode.wspr));
      item.insert (QStringLiteral ("drift"), decode.wspr ? QJsonValue {decode.drift} : QJsonValue {QJsonValue::Null});
      item.insert (QStringLiteral ("power"), decode.wspr ? QJsonValue {decode.power} : QJsonValue {QJsonValue::Null});
      item.insert (QStringLiteral ("source_revision"), static_cast<qint64> (decode.source_revision));
      decode_array.append (item);
    }
  object.insert (QStringLiteral ("recent_decodes"), decode_array);
  return object;
}
