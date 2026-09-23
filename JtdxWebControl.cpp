#include "JtdxWebControl.hpp"
#include "JtdxWebDx.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

namespace
{
  constexpr int max_request_id_length = 64;
  constexpr int max_target_length = 32;
  constexpr int max_grid_length = 10;
  constexpr int timer_interval_ms = 25;
}

JtdxWebControl::JtdxWebControl (qint64 timeout_ms, Clock clock, QObject * parent)
  : QObject {parent}
  , clock_ {std::move (clock)}
  , timeout_ms_ {qMax<qint64> (1, timeout_ms)}
  , epoch_ {QUuid::createUuid ().toString (QUuid::WithoutBraces)}
{
  elapsed_clock_.start ();
  if (clock_) test_clock_ = true;
  expiry_timer_.setSingleShot (false);
  expiry_timer_.setInterval (timer_interval_ms);
  connect (&expiry_timer_, &QTimer::timeout, this, &JtdxWebControl::on_timer);
}

QString JtdxWebControl::operation_name (Operation operation)
{
  switch (operation)
    {
    case Operation::Frequency: return QStringLiteral ("frequency");
    case Operation::SelectDx: return QStringLiteral ("select-dx");
    case Operation::StartCq: return QStringLiteral ("start-cq");
    case Operation::StartAutoCall: return QStringLiteral ("start-auto-call");
    case Operation::StopAutoCall: return QStringLiteral ("stop-auto-call");
    case Operation::Radio: return QStringLiteral ("radio");
    }
  return QStringLiteral ("unknown");
}

QString JtdxWebControl::status_name (Status status)
{
  switch (status)
    {
    case Status::Received: return QStringLiteral ("received");
    case Status::Accepted: return QStringLiteral ("accepted");
    case Status::Pending: return QStringLiteral ("pending");
    case Status::Completed: return QStringLiteral ("completed");
    case Status::Failed: return QStringLiteral ("failed");
    case Status::Rejected: return QStringLiteral ("rejected");
    case Status::Timeout: return QStringLiteral ("timeout");
    }
  return QStringLiteral ("unknown");
}

QString JtdxWebControl::normalize_request_id (QString const& request_id)
{
  QString const value = request_id.trimmed ();
  if (!printable_ascii (value, max_request_id_length)) return {};
  return value;
}

bool JtdxWebControl::printable_ascii (QString const& value, int max_length)
{
  if (value.isEmpty () || value.size () > max_length) return false;
  for (QChar const character : value)
    if (character.unicode () < 0x21 || character.unicode () > 0x7e)
      return false;
  return true;
}

bool JtdxWebControl::printable_radio_text (QString const& value, int max_length)
{
  if (value.size () > max_length) return false;
  for (QChar const character : value)
    if (character.unicode () < 0x20 || character.unicode () > 0x7e)
      return false;
  return true;
}

QString JtdxWebControl::canonical_payload (Request const& request)
{
  if (request.operation == Operation::Frequency)
    return QStringLiteral ("frequency:") + QString::number (request.frequency_hz);
  if (request.operation == Operation::StartCq || request.operation == Operation::StartAutoCall
      || request.operation == Operation::StopAutoCall)
    return operation_name (request.operation);
  if (request.operation == Operation::Radio)
    return QStringLiteral ("radio:") + request.radio_action + QStringLiteral (":")
        + (request.radio_value ? QStringLiteral ("1") : QStringLiteral ("0"))
        + QStringLiteral (":") + QString::number (request.radio_index)
        + QStringLiteral (":") + request.radio_text + QStringLiteral (":")
        + QString::fromUtf8 (QJsonDocument {request.radio_qso}.toJson (QJsonDocument::Compact));
  QString const call = request.dx_call.trimmed ().toUpper ();
  QString const grid = request.dx_grid.trimmed ().toUpper ();
  return QStringLiteral ("select-dx:") + QString::number (call.size ()) + QStringLiteral (":") + call
      + QString::number (grid.size ()) + QStringLiteral (":") + grid
      + QStringLiteral (":") + request.dx_selection_source
      + QStringLiteral (":") + QString::number (request.dx_source_decode_id);
}

bool JtdxWebControl::safe_to_dispatch (SafetySnapshot const& safety, Operation operation, QString * reason)
{
  // Stop is a fail-safe action: it must remain admissible while TX/PTT or a
  // watchdog path is active so the existing Halt/stop entry can quiesce it.
  // It does not unlock or start any unsafe path.
  if (operation == Operation::StopAutoCall) return true;
  if (!safety.known) *reason = QStringLiteral ("safety_unknown");
  else if (!safety.rig_online) *reason = QStringLiteral ("rig_offline");
  else if (!safety.monitoring) *reason = QStringLiteral ("monitor_not_active");
  else if (safety.transmitting) *reason = QStringLiteral ("transmitting");
  else if (safety.ptt) *reason = QStringLiteral ("ptt_active");
  else if (safety.tx_enabled) *reason = QStringLiteral ("tx_enabled");
  else if (safety.watchdog_timeout) *reason = QStringLiteral ("watchdog_timeout");
  else if (safety.start2) *reason = QStringLiteral ("startup_pending");
  else if (safety.tune || safety.auto_tx || safety.iptt)
    *reason = QStringLiteral ("tx_path_active");
  else if (!safety.business_state_known) *reason = QStringLiteral ("business_state_unknown");
  else return true;
  return false;
}

qint64 JtdxWebControl::now () const
{
  return clock_ ? clock_ () : (test_clock_ ? test_now_ms_ : elapsed_clock_.elapsed ());
}

JtdxWebControl::ObservedState JtdxWebControl::observation () const
{
  if (observation_provider_) return observation_provider_ ();
  return observed_;
}

JtdxWebControl::Result JtdxWebControl::result (QString const& request_id) const
{
  auto const it = records_.constFind (request_id);
  return it == records_.constEnd () ? Result {} : it.value ().result;
}

QVector<JtdxWebControl::Result> JtdxWebControl::operation_results () const
{
  QVector<Result> results;
  results.reserve (records_.size ());
  for (auto const& record : records_)
    results.append (record.result);
  std::sort (results.begin (), results.end (), [] (Result const& left, Result const& right) {
    if (left.received_ms != right.received_ms) return left.received_ms < right.received_ms;
    return left.request_id < right.request_id;
  });
  return results;
}

void JtdxWebControl::mark_operations_changed ()
{
  ++operation_revision_;
}

void JtdxWebControl::set_observed_state (ObservedState state)
{
  observed_ = std::move (state);
}

void JtdxWebControl::set_observation_provider (ObservationProvider provider)
{
  observation_provider_ = std::move (provider);
}

JtdxWebControl::ObservedState JtdxWebControl::observed_state () const
{
  return observation ();
}

void JtdxWebControl::set_frequency_dispatcher (DispatchHandler handler)
{
  frequency_dispatcher_ = std::move (handler);
}

void JtdxWebControl::set_select_dx_dispatcher (DispatchHandler handler)
{
  select_dx_dispatcher_ = std::move (handler);
}

void JtdxWebControl::bind_server_epoch (QString server_epoch)
{
  server_epoch = server_epoch.trimmed ();
  if (shutdown_ || server_epoch.isEmpty ()) return;

  if (server_epoch_bound_ && epoch_ == server_epoch) return;

  // 成功监听产生新 epoch；任何残留 pending 都属于旧服务，先失效再绑定。
  if (!pending_request_id_.isEmpty ())
    invalidate_server_epoch (QStringLiteral ("server_epoch_changed"));
  if (!server_epoch_bound_ || epoch_ != server_epoch)
    {
      if (!records_.isEmpty ()) mark_operations_changed ();
      records_.clear ();
    }
  epoch_ = std::move (server_epoch);
  server_epoch_bound_ = true;
}

void JtdxWebControl::invalidate_server_epoch (QString reason)
{
  if (shutdown_) return;
  if (!pending_request_id_.isEmpty ())
    {
      auto it = records_.find (pending_request_id_);
      if (it != records_.end ())
        {
          bool const was_dispatched = it.value ().dispatched;
          QString const request_id = it.value ().result.request_id;
          finish (it.value (), Status::Rejected, std::move (reason));
          if (was_dispatched)
            {
              // 已经开始的外部操作可能仍会生效；服务重启不能绕过这个锁。
              unconfirmed_latch_ = true;
              last_timed_out_request_id_ = request_id;
            }
        }
      else
        {
          pending_request_id_.clear ();
          expiry_timer_.stop ();
        }
    }
  server_epoch_bound_ = false;
}

void JtdxWebControl::log_result (Result const& result, QString event) const
{
  if (!diagnostic_logger_) return;
  auto const escaped = [] (QString value) {
    value.replace ('\\', QStringLiteral ("\\\\"));
    value.replace ('\r', QLatin1Char (' '));
    value.replace ('\n', QLatin1Char (' '));
    value.replace ('\t', QLatin1Char (' '));
    value.replace ('=', QStringLiteral ("%3D"));
    return value.left (180);
  };
  QString const normalized_request_id = normalize_request_id (result.request_id);
  QString const request_id = normalized_request_id.isEmpty () ? QStringLiteral ("-")
                                                               : escaped (normalized_request_id);
  QString const reason = result.reason.isEmpty () ? QStringLiteral ("-") : escaped (result.reason);
  QString const cq_state = result.snapshot.cq_state.isEmpty ()
      ? QStringLiteral ("-") : escaped (result.snapshot.cq_state);
  QString line = QStringLiteral ("event=%1 request_id=%2 operation=%3 status=%4 reason=%5 "
                                 "generation=%6 state_revision=%7 readback=%8 "
                                 "frequency_known=%9 actual_frequency_hz=%10 dx_known=%11 "
                                 "business_state_known=%12 cq_state=%13 auto_sequence_enabled=%14")
      .arg (escaped (std::move (event)))
      .arg (request_id)
      .arg (operation_name (result.operation))
      .arg (status_name (result.status))
      .arg (reason)
      .arg (QString::number (result.generation))
      .arg (QString::number (result.snapshot.state_revision))
      .arg (result.status == Status::Completed ? QStringLiteral ("confirmed")
                                                : QStringLiteral ("unknown"))
      .arg (result.snapshot.frequency_known ? QStringLiteral ("1") : QStringLiteral ("0"))
      .arg (QString::number (result.snapshot.actual_frequency_hz))
      .arg (result.snapshot.dx_known ? QStringLiteral ("1") : QStringLiteral ("0"))
      .arg (result.snapshot.business_state_known ? QStringLiteral ("1") : QStringLiteral ("0"))
      .arg (cq_state)
      .arg (result.snapshot.auto_sequence_enabled ? QStringLiteral ("1") : QStringLiteral ("0"));
  try { diagnostic_logger_ (line); } catch (...) {}
}

JtdxWebControl::Result JtdxWebControl::reject (Request const& request, QString reason,
                                               int http_status)
{
  Result result;
  result.request_id = request.request_id;
  result.operation = request.operation;
  result.status = Status::Rejected;
  result.reason = std::move (reason);
  result.server_epoch = epoch_;
  result.http_status = http_status;
  result.received_ms = now ();
  result.snapshot = observed_;
  result.initial_state_revision = observed_.state_revision;
  log_result (result, QStringLiteral ("reject"));
  return result;
}

void JtdxWebControl::finish (Record& record, Status status, QString reason, quint64 generation)
{
  Status const previous_status = record.result.status;
  record.result.status = status;
  record.result.reason = std::move (reason);
  record.result.generation = generation;
  record.result.completed_ms = status == Status::Pending ? -1 : now ();
  if (previous_status != status && record.result.status_history.size () < 4)
    record.result.status_history.append (status);
  record.result.http_status = status == Status::Completed ? 200
      : status == Status::Pending || status == Status::Accepted ? 202
      : status == Status::Timeout ? 504 : status == Status::Rejected ? 409 : 502;
  if (status != Status::Pending && pending_request_id_ == record.result.request_id)
    {
      pending_request_id_.clear ();
      expiry_timer_.stop ();
    }
  mark_operations_changed ();
  log_result (record.result, QStringLiteral ("transition"));
}

JtdxWebControl::Result JtdxWebControl::submit (Request request)
{
  if (shutdown_) return reject (request, QStringLiteral ("shutdown"), 409);
  if (!server_epoch_bound_) return reject (request, QStringLiteral ("server_unavailable"), 409);
  request.request_id = normalize_request_id (request.request_id);
  if (request.request_id.isEmpty ()) return reject (request, QStringLiteral ("invalid_request_id"), 400);
  if (request.server_epoch != epoch_) return reject (request, QStringLiteral ("epoch_conflict"), 409);
  if (request.operation == Operation::Frequency)
    {
      if (request.frequency_hz <= 0) return reject (request, QStringLiteral ("invalid_frequency"), 400);
    }
  else if (request.operation == Operation::SelectDx)
    {
      request.dx_call = request.dx_call.trimmed ().toUpper ();
      request.dx_grid = request.dx_grid.trimmed ().toUpper ();
      if (!printable_ascii (request.dx_call, max_target_length)
          || (!request.dx_grid.isEmpty () && !printable_ascii (request.dx_grid, max_grid_length)))
        return reject (request, QStringLiteral ("invalid_normalized_dx"), 400);
      auto const normalized = JtdxWebDx::normalize (request.dx_call, request.dx_grid);
      if (!normalized.valid) return reject (request, normalized.reason, 400);
      request.dx_call = normalized.call;
      request.dx_grid = normalized.grid;
    }
  else if (request.operation == Operation::Radio)
    {
      static const QSet<QString> actions {
        QStringLiteral ("enable-tx"), QStringLiteral ("stop-tx"),
        QStringLiteral ("log-qso"), QStringLiteral ("clear-windows"),
        QStringLiteral ("sync"), QStringLiteral ("multi-decode"),
        QStringLiteral ("agc-compensation"), QStringLiteral ("narrow"),
        QStringLiteral ("decode"), QStringLiteral ("clear-dx"),
        QStringLiteral ("generate-message"), QStringLiteral ("cq"),
        QStringLiteral ("skip-tx1"), QStringLiteral ("select-tx"),
        QStringLiteral ("set-tx-message"), QStringLiteral ("log-qso-confirm"),
        QStringLiteral ("log-qso-cancel")};
      if (!actions.contains (request.radio_action))
        return reject (request, QStringLiteral ("invalid_radio_action"), 400);
      if (request.radio_index < 0 || request.radio_index > 6
          || !printable_radio_text (request.radio_text, 64))
        return reject (request, QStringLiteral ("invalid_radio_fields"), 400);
      if ((request.radio_action == QStringLiteral ("select-tx")
           || request.radio_action == QStringLiteral ("set-tx-message"))
          && (request.radio_index < 1 || request.radio_index > 6))
        return reject (request, QStringLiteral ("invalid_tx_index"), 400);
      if (request.radio_action == QStringLiteral ("log-qso-confirm")
          && request.radio_qso.isEmpty ())
        return reject (request, QStringLiteral ("qso_draft_required"), 400);
    }

  QString const request_epoch = epoch_;

  auto const existing = records_.constFind (request.request_id);
  if (existing != records_.constEnd ())
    {
      Request prior;
      prior.request_id = existing.value ().result.request_id;
      prior.operation = existing.value ().result.operation;
      if (existing.value ().canonical_payload == canonical_payload (request)
          && prior.operation == request.operation)
        {
          log_result (existing.value ().result, QStringLiteral ("duplicate"));
          return existing.value ().result;
        }
      return reject (request, QStringLiteral ("request_id_conflict"), 409);
    }
  // An already-begun operation may still have taken effect after its feedback
  // deadline.  Keep the latch for every new start/change, but preserve the
  // existing fail-safe Stop path so the caller can quiesce that unknown work.
  if (unconfirmed_latch_ && request.operation != Operation::StopAutoCall)
    return reject (request, QStringLiteral ("unconfirmed_feedback"), 409);
  if (records_.size () >= hard_record_limit_)
    return reject (request, QStringLiteral ("record_limit"), 429);
  if (!pending_request_id_.isEmpty ())
    {
      auto pending = records_.find (pending_request_id_);
      bool const business_start = pending != records_.end ()
          && (pending.value ().result.operation == Operation::StartCq
              || pending.value ().result.operation == Operation::StartAutoCall);
      if (request.operation != Operation::StopAutoCall || !business_start)
        return reject (request, QStringLiteral ("busy"), 409);
      // A stop is the recovery path for a queued or already-started business
      // command.  The old callback will fail its pending/epoch checks, while
      // the existing stop entry is allowed to quiesce any side effect.
      finish (pending.value (), Status::Rejected, QStringLiteral ("superseded_by_stop"));
    }

  if (submit_in_progress_)
    return reject (request, QStringLiteral ("reentrant"), 409);

  ObservedState current;
  {
    struct SubmitGuard
    {
      bool& active;
      ~SubmitGuard () { active = false; }
    };
    submit_in_progress_ = true;
    SubmitGuard const submit_guard {submit_in_progress_};
    try
      {
        current = observation ();
      }
    catch (...)
      {
        return reject (request, QStringLiteral ("observation_unavailable"), 503);
      }
  }
  if (shutdown_ || !server_epoch_bound_ || epoch_ != request_epoch)
    return reject (request, QStringLiteral ("epoch_changed"), 409);
  QString safety_reason;
  if (!safe_to_dispatch (current.safety, request.operation, &safety_reason)
      && !(request.operation == Operation::Radio && request.radio_action == QStringLiteral ("stop-tx")))
    return reject (request, safety_reason, 409);

  Record record;
  record.result.request_id = request.request_id;
  record.result.operation = request.operation;
  record.result.status = Status::Received;
  record.result.server_epoch = epoch_;
  record.canonical_payload = canonical_payload (request);
  record.received_ms = now ();
  record.deadline_ms = record.received_ms + timeout_ms_;
  record.frequency_hz = request.frequency_hz;
  record.dx_call = request.dx_call;
  record.dx_grid = request.dx_grid;
  record.dx_report = request.dx_report;
  record.dx_frequency_offset = request.dx_frequency_offset;
  record.dx_time = request.dx_time;
  record.dx_selection_source = request.dx_selection_source;
  record.dx_source_decode_id = request.dx_source_decode_id;
  record.radio_action = request.radio_action;
  record.radio_value = request.radio_value;
  record.radio_index = request.radio_index;
  record.radio_text = request.radio_text;
  record.radio_qso = request.radio_qso;
  record.baseline_business_generation = current.business_generation;
  record.target_cq_state = request.operation == Operation::StartCq ? QStringLiteral ("armed")
      : request.operation == Operation::StopAutoCall ? QStringLiteral ("idle") : QString {};
  record.target_auto_sequence_enabled = request.operation == Operation::StartAutoCall;
  records_.insert (request.request_id, record);
  mark_operations_changed ();
  auto inserted = records_.find (request.request_id);
  inserted.value ().result.received_ms = record.received_ms;
  inserted.value ().result.deadline_ms = record.deadline_ms;
  inserted.value ().result.initial_state_revision = current.state_revision;
  inserted.value ().result.snapshot = current;
  inserted.value ().result.status_history.append (Status::Received);

  auto stored_it = records_.find (request.request_id);
  if (shutdown_ || !server_epoch_bound_ || epoch_ != request_epoch
      || stored_it == records_.end ())
    {
      auto gone = records_.constFind (request.request_id);
      return gone == records_.constEnd () ? reject (request, QStringLiteral ("epoch_changed"), 409)
                                           : gone.value ().result;
    }
  Record& stored = stored_it.value ();
  stored.result.snapshot = current;
  stored.baseline_generation = request.operation == Operation::Frequency
      ? current.frequency_generation : request.operation == Operation::SelectDx
        ? current.dx_generation : request.operation == Operation::Radio
          ? current.state_revision : current.business_generation;
  stored.baseline_state_revision = current.state_revision;
  bool const already_matches = request.operation == Operation::Frequency
      ? (current.frequency_known && current.actual_frequency_hz == request.frequency_hz)
      : request.operation == Operation::SelectDx
        ? (current.dx_known && current.dx_call.trimmed ().toUpper () == request.dx_call
           && current.dx_grid.trimmed ().toUpper () == request.dx_grid)
      : request.operation == Operation::Radio ? false
      : (current.business_state_known
           && current.auto_sequence_enabled == (request.operation == Operation::StartAutoCall)
           && (request.operation != Operation::StartCq || current.cq_state == QStringLiteral ("armed"))
           && (request.operation != Operation::StopAutoCall || current.cq_state == QStringLiteral ("idle")));
  if (already_matches)
    {
      finish (stored, Status::Completed,
              request.operation == Operation::Frequency ? QStringLiteral ("already_at_frequency")
                                                        : QStringLiteral ("already_selected"),
              stored.baseline_generation);
      return stored.result;
    }

  stored.result.status = Status::Accepted;
  stored.result.http_status = 202;
  stored.result.reason = QStringLiteral ("registered");
  stored.result.status_history.append (Status::Accepted);
  stored.result.status = Status::Pending;
  stored.result.reason = QStringLiteral ("awaiting_feedback");
  stored.result.status_history.append (Status::Pending);
  log_result (stored.result, QStringLiteral ("accepted"));
  pending_request_id_ = request.request_id;
  arm_timer ();

  Dispatch dispatch;
  dispatch.request_id = request.request_id;
  dispatch.server_epoch = epoch_;
  dispatch.operation = request.operation;
  dispatch.expected_generation = stored.baseline_generation;
  dispatch.frequency_hz = request.frequency_hz;
  dispatch.dx_call = request.dx_call;
  dispatch.dx_grid = request.dx_grid;
  dispatch.dx_report = request.dx_report;
  dispatch.dx_frequency_offset = request.dx_frequency_offset;
  dispatch.dx_time = request.dx_time;
  dispatch.dx_selection_source = request.dx_selection_source;
  dispatch.dx_source_decode_id = request.dx_source_decode_id;
  dispatch.radio_action = request.radio_action;
  dispatch.radio_value = request.radio_value;
  dispatch.radio_index = request.radio_index;
  dispatch.radio_text = request.radio_text;
  dispatch.radio_qso = request.radio_qso;
  DispatchHandler handler = request.operation == Operation::Frequency ? frequency_dispatcher_
      : request.operation == Operation::SelectDx ? select_dx_dispatcher_ : business_dispatcher_;
  try
    {
      if (!handler)
        {
          auto missing = records_.find (request.request_id);
          if (missing != records_.end () && missing.value ().result.status == Status::Pending)
            finish (missing.value (), Status::Failed, QStringLiteral ("dispatch_unavailable"));
        }
      else
        handler (dispatch);
    }
    catch (...)
    {
      auto failed = records_.find (request.request_id);
      if (epoch_ == request_epoch && failed != records_.end ()
          && failed.value ().result.server_epoch == request_epoch
          && failed.value ().result.status == Status::Pending)
        finish (failed.value (), Status::Failed, QStringLiteral ("dispatch_exception"));
    }
  if (!server_epoch_bound_ || epoch_ != request_epoch)
    return reject (request, QStringLiteral ("epoch_changed"), 409);
  auto completed = records_.constFind (request.request_id);
  return completed == records_.constEnd () ? reject (request, QStringLiteral ("record_lost"))
                                             : completed.value ().result;
}

bool JtdxWebControl::prepare_dispatch (QString const& request_id, QString const& server_epoch,
                                       Dispatch * prepared)
{
  if (!prepared || shutdown_ || !server_epoch_bound_ || server_epoch != epoch_
      || pending_request_id_ != request_id)
    return false;
  expire ();
  auto it = records_.find (request_id);
  if (it == records_.end () || it.value ().result.server_epoch != server_epoch
      || it.value ().result.status != Status::Pending || it.value ().dispatched)
    return false;
  if (now () >= it.value ().deadline_ms)
    {
      expire ();
      return false;
    }

  ObservedState current;
  {
    struct ObservationGuard
    {
      bool& active;
      ~ObservationGuard () { active = false; }
    };
    if (submit_in_progress_) return false;
    submit_in_progress_ = true;
    ObservationGuard const guard {submit_in_progress_};
    try
      {
        current = observation ();
      }
    catch (...)
      {
        auto failed = records_.find (request_id);
        if (failed != records_.end () && failed.value ().result.server_epoch == server_epoch
            && failed.value ().result.status == Status::Pending)
          finish (failed.value (), Status::Failed, QStringLiteral ("observation_unavailable"));
        return false;
      }
  }

  // 外部 provider 可能在回调期间 shutdown/rotate；重新按 key 查找，避免悬空 Record&。
  it = records_.find (request_id);
  if (shutdown_ || !server_epoch_bound_ || epoch_ != server_epoch || pending_request_id_ != request_id
      || it == records_.end () || it.value ().result.server_epoch != server_epoch
      || it.value ().result.status != Status::Pending || it.value ().dispatched)
    return false;
  if (now () >= it.value ().deadline_ms)
    {
      expire ();
      return false;
    }
  QString safety_reason;
  if (!safe_to_dispatch (current.safety, it.value ().result.operation, &safety_reason)
      && !(it.value ().result.operation == Operation::Radio
           && it.value ().radio_action == QStringLiteral ("stop-tx")))
    {
      finish (it.value (), Status::Rejected, std::move (safety_reason));
      return false;
    }

  if (it.value ().result.operation == Operation::Frequency)
    {
      if (!current.frequency_known || current.actual_frequency_hz <= 0
          || current.frequency_generation == 0)
        {
          finish (it.value (), Status::Rejected, QStringLiteral ("frequency_baseline_unknown"));
          return false;
        }
      if (current.actual_frequency_hz == it.value ().frequency_hz)
        {
          finish (it.value (), Status::Completed, QStringLiteral ("already_at_frequency"),
                  current.frequency_generation);
          return false;
        }
    }

  it.value ().result.snapshot = current;
  it.value ().baseline_generation = it.value ().result.operation == Operation::Frequency
      ? current.frequency_generation : it.value ().result.operation == Operation::SelectDx
        ? current.dx_generation : it.value ().result.operation == Operation::Radio
          ? current.state_revision : current.business_generation;
  it.value ().baseline_state_revision = current.state_revision;
  it.value ().prepared = true;

  Dispatch result;
  result.request_id = request_id;
  result.server_epoch = server_epoch;
  result.operation = it.value ().result.operation;
  result.expected_generation = it.value ().baseline_generation;
  result.frequency_hz = it.value ().frequency_hz;
  result.dx_call = it.value ().dx_call;
  result.dx_grid = it.value ().dx_grid;
  result.dx_report = it.value ().dx_report;
  result.dx_frequency_offset = it.value ().dx_frequency_offset;
  result.dx_time = it.value ().dx_time;
  result.dx_selection_source = it.value ().dx_selection_source;
  result.dx_source_decode_id = it.value ().dx_source_decode_id;
  result.radio_action = it.value ().radio_action;
  result.radio_value = it.value ().radio_value;
  result.radio_index = it.value ().radio_index;
  result.radio_text = it.value ().radio_text;
  result.radio_qso = it.value ().radio_qso;
  *prepared = std::move (result);
  return true;
}

bool JtdxWebControl::begin_dispatch (Dispatch const& dispatch)
{
  if (shutdown_ || !server_epoch_bound_ || dispatch.server_epoch != epoch_
      || pending_request_id_ != dispatch.request_id)
    return false;
  expire ();
  auto it = records_.find (dispatch.request_id);
  if (it == records_.end () || it.value ().result.server_epoch != dispatch.server_epoch
      || it.value ().result.status != Status::Pending || !it.value ().prepared
      || it.value ().dispatched)
    return false;
  if (now () >= it.value ().deadline_ms)
    {
      expire ();
      return false;
    }
  ObservedState current;
  {
    struct ObservationGuard
    {
      bool& active;
      ~ObservationGuard () { active = false; }
    };
    if (submit_in_progress_) return false;
    submit_in_progress_ = true;
    ObservationGuard const guard {submit_in_progress_};
    try
      {
        current = observation ();
      }
    catch (...)
      {
        return false;
      }
  }
  it = records_.find (dispatch.request_id);
  if (shutdown_ || !server_epoch_bound_ || epoch_ != dispatch.server_epoch
      || pending_request_id_ != dispatch.request_id
      || it == records_.end () || it.value ().result.status != Status::Pending
      || !it.value ().prepared || it.value ().dispatched)
    return false;
  QString safety_reason;
  if (!safe_to_dispatch (current.safety, it.value ().result.operation, &safety_reason)
      && !(it.value ().result.operation == Operation::Radio
           && it.value ().radio_action == QStringLiteral ("stop-tx")))
    {
      finish (it.value (), Status::Rejected, std::move (safety_reason));
      return false;
    }
  if (now () >= it.value ().deadline_ms)
    {
      expire ();
      return false;
    }
  quint64 const current_generation = dispatch.operation == Operation::Frequency
      ? current.frequency_generation : dispatch.operation == Operation::SelectDx
        ? current.dx_generation : dispatch.operation == Operation::Radio
          ? current.state_revision : current.business_generation;
  if (current_generation != it.value ().baseline_generation)
    {
      it.value ().prepared = false;
      return false;
    }
  if (dispatch.operation != it.value ().result.operation
      || dispatch.expected_generation != it.value ().baseline_generation
      || dispatch.frequency_hz != it.value ().frequency_hz
      || dispatch.dx_call != it.value ().dx_call || dispatch.dx_grid != it.value ().dx_grid
      || dispatch.dx_selection_source != it.value ().dx_selection_source
      || dispatch.dx_source_decode_id != it.value ().dx_source_decode_id
      || dispatch.radio_action != it.value ().radio_action
      || dispatch.radio_value != it.value ().radio_value
      || dispatch.radio_index != it.value ().radio_index
      || dispatch.radio_text != it.value ().radio_text
      || dispatch.radio_qso != it.value ().radio_qso)
    return false;
  it.value ().dispatched = true;
  return true;
}

bool JtdxWebControl::feedback_frequency (QString const& request_id, QString const& server_epoch,
                                         quint64 generation, qint64 actual_frequency_hz,
                                         quint64 state_revision)
{
  if (!server_epoch_bound_) return false;
  if (pending_request_id_.isEmpty ())
    {
      if (server_epoch == epoch_ && request_id == last_timed_out_request_id_)
        {
          auto late = records_.find (last_timed_out_request_id_);
          if (late != records_.end () && late.value ().timed_out && late.value ().dispatched)
            {
              late.value ().result.reason = QStringLiteral ("feedback_timeout/late_feedback_unknown");
              mark_operations_changed ();
            }
        }
      return false;
    }
  if (request_id != pending_request_id_ || server_epoch != epoch_) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || it.value ().result.operation != Operation::Frequency
      || !it.value ().dispatched) return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
      unconfirmed_latch_ = true;
      last_timed_out_request_id_ = request_id;
      return false;
    }
  if (record.result.server_epoch != epoch_ || generation <= record.baseline_generation
      || state_revision <= record.baseline_state_revision || actual_frequency_hz <= 0
      || actual_frequency_hz != record.frequency_hz) return false;
  record.result.snapshot = observed_;
  record.result.snapshot.frequency_known = true;
  record.result.snapshot.actual_frequency_hz = actual_frequency_hz;
  record.result.snapshot.frequency_generation = generation;
  record.result.snapshot.state_revision = state_revision;
  record.result.snapshot.safety = observed_.safety;
  finish (record, Status::Completed, QStringLiteral ("feedback_matched"), generation);
  return true;
}

bool JtdxWebControl::feedback_select_dx (QString const& request_id, QString const& server_epoch,
                                         quint64 generation, QString dx_call, QString dx_grid,
                                         quint64 state_revision, QString dx_report,
                                         qint32 dx_frequency_offset, QString dx_time,
                                         QString dx_selection_source, quint64 dx_source_decode_id)
{
  if (!server_epoch_bound_) return false;
  if (pending_request_id_.isEmpty ())
    {
      if (server_epoch == epoch_ && request_id == last_timed_out_request_id_)
        {
          auto late = records_.find (last_timed_out_request_id_);
          if (late != records_.end () && late.value ().timed_out && late.value ().dispatched)
            {
              late.value ().result.reason = QStringLiteral ("feedback_timeout/late_feedback_unknown");
              mark_operations_changed ();
            }
        }
      return false;
    }
  if (request_id != pending_request_id_ || server_epoch != epoch_) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || it.value ().result.operation != Operation::SelectDx
      || !it.value ().dispatched) return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
      unconfirmed_latch_ = true;
      last_timed_out_request_id_ = request_id;
      return false;
    }
  if (record.result.server_epoch != epoch_ || generation <= record.baseline_generation
      || state_revision <= record.baseline_state_revision) return false;
  dx_call = dx_call.trimmed ().toUpper ();
  dx_grid = dx_grid.trimmed ().toUpper ();
  if (dx_call != record.dx_call || dx_grid != record.dx_grid) return false;
  if (!record.dx_selection_source.isEmpty () && dx_selection_source != record.dx_selection_source)
    return false;
  if (record.dx_source_decode_id != 0 && dx_source_decode_id != record.dx_source_decode_id)
    return false;
  record.result.snapshot = observed_;
  record.result.snapshot.dx_known = true;
  record.result.snapshot.dx_call = dx_call;
  record.result.snapshot.dx_grid = dx_grid;
  record.result.snapshot.dx_report = dx_report;
  record.result.snapshot.dx_frequency_offset = dx_frequency_offset;
  record.result.snapshot.dx_time = dx_time;
  record.result.snapshot.dx_selection_source = dx_selection_source;
  record.result.snapshot.dx_source_decode_id = dx_source_decode_id;
  record.result.snapshot.dx_generation = generation;
  record.result.snapshot.state_revision = state_revision;
  QString reason = QStringLiteral ("feedback_matched");
  if (record.result.operation == Operation::StartCq)
    reason = QStringLiteral ("cq_armed");
  else if (record.result.operation == Operation::StartAutoCall)
    reason = QStringLiteral ("auto_sequence_armed_waiting_for_decode");
  else if (record.result.operation == Operation::StopAutoCall)
    reason = QStringLiteral ("automation_stopped");
  finish (record, Status::Completed, std::move (reason), generation);
  return true;
}

bool JtdxWebControl::feedback_business (QString const& request_id, QString const& server_epoch,
                                        quint64 generation, QString cq_state,
                                        bool auto_sequence_enabled, quint64 state_revision)
{
  if (!server_epoch_bound_ || pending_request_id_.isEmpty ()
      || request_id != pending_request_id_ || server_epoch != epoch_)
    return false;
  auto it = records_.find (request_id);
  if (it == records_.end () || !it.value ().dispatched
      || (it.value ().result.operation != Operation::StartCq
          && it.value ().result.operation != Operation::StartAutoCall
          && it.value ().result.operation != Operation::StopAutoCall)) return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
      unconfirmed_latch_ = true;
      last_timed_out_request_id_ = request_id;
      return false;
    }
  if (generation <= record.baseline_generation || state_revision <= record.baseline_state_revision)
    return false;
  bool const cq_matches = record.result.operation == Operation::StartCq
      ? cq_state == QStringLiteral ("armed") : record.result.operation == Operation::StopAutoCall
        ? (cq_state == QStringLiteral ("idle") || cq_state == QStringLiteral ("not_selected")) : true;
  bool const auto_matches = record.result.operation == Operation::StartAutoCall
      ? auto_sequence_enabled : record.result.operation == Operation::StopAutoCall
        ? !auto_sequence_enabled : true;
  if (!cq_matches || !auto_matches) return false;
  record.result.snapshot = observed_;
  record.result.snapshot.business_generation = generation;
  record.result.snapshot.business_state_known = true;
  record.result.snapshot.cq_state = cq_state;
  record.result.snapshot.auto_sequence_enabled = auto_sequence_enabled;
  record.result.snapshot.state_revision = state_revision;
  QString reason = QStringLiteral ("feedback_matched");
  if (record.result.operation == Operation::StartCq)
    reason = QStringLiteral ("cq_armed");
  else if (record.result.operation == Operation::StartAutoCall)
    reason = QStringLiteral ("auto_sequence_armed_waiting_for_decode");
  else if (record.result.operation == Operation::StopAutoCall)
    reason = QStringLiteral ("automation_stopped");
  finish (record, Status::Completed, std::move (reason), generation);
  return true;
}

bool JtdxWebControl::feedback_radio (QString const& request_id, QString const& server_epoch,
                                     quint64 state_revision, ObservedState const& observed)
{
  if (!server_epoch_bound_ || pending_request_id_.isEmpty ()
      || request_id != pending_request_id_ || server_epoch != epoch_)
    return false;
  auto it = records_.find (request_id);
  if (it == records_.end () || !it.value ().dispatched
      || it.value ().result.operation != Operation::Radio)
    return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
      unconfirmed_latch_ = true;
      last_timed_out_request_id_ = request_id;
      return false;
    }
  if (state_revision <= record.baseline_state_revision || !observed.radio_state_known)
    return false;
  bool matched = true;
  if (record.radio_action == QStringLiteral ("enable-tx")) matched = observed.safety.tx_enabled;
  else if (record.radio_action == QStringLiteral ("stop-tx")) matched = !observed.safety.tx_enabled;
  else if (record.radio_action == QStringLiteral ("multi-decode")) matched = observed.radio_multi_decode == record.radio_value;
  else if (record.radio_action == QStringLiteral ("agc-compensation")) matched = observed.radio_agc_compensation == record.radio_value;
  else if (record.radio_action == QStringLiteral ("narrow")) matched = observed.radio_narrow == record.radio_value;
  else if (record.radio_action == QStringLiteral ("sync")) matched = observed.radio_sync == record.radio_value;
  else if (record.radio_action == QStringLiteral ("skip-tx1")) matched = observed.radio_skip_tx1 == record.radio_value;
  else if (record.radio_action == QStringLiteral ("select-tx")) matched = observed.radio_current_tx_index == record.radio_index;
  else if (record.radio_action == QStringLiteral ("set-tx-message"))
    matched = record.radio_index >= 1 && record.radio_index <= observed.radio_tx_messages.size ()
      && observed.radio_tx_messages.at (record.radio_index - 1) == record.radio_text;
  else if (record.radio_action == QStringLiteral ("log-qso")) matched = !observed.radio_qso_draft.isEmpty ()
      && observed.radio_log_dialog_open;
  else if (record.radio_action == QStringLiteral ("log-qso-cancel")) matched = !observed.radio_log_dialog_open;
  else if (record.radio_action == QStringLiteral ("log-qso-confirm"))
    matched = !observed.radio_log_dialog_open
      && observed.radio_qso_generation > record.result.snapshot.radio_qso_generation;
  else if (record.radio_action == QStringLiteral ("cq")) matched = observed.cq_state == QStringLiteral ("armed");
  if (!matched) return false;
  record.result.snapshot = observed;
  record.result.snapshot.state_revision = state_revision;
  finish (record, Status::Completed, QStringLiteral ("readback_matched"), state_revision);
  return true;
}

bool JtdxWebControl::fail (QString const& request_id, QString const& server_epoch, QString reason)
{
  if (!server_epoch_bound_ || pending_request_id_.isEmpty () || request_id != pending_request_id_
      || server_epoch != epoch_)
    return false;
  auto it = records_.find (request_id);
  if (it == records_.end () || it.value ().result.server_epoch != epoch_)
    return false;
  finish (it.value (), Status::Failed, std::move (reason));
  return true;
}

bool JtdxWebControl::expire ()
{
  if (pending_request_id_.isEmpty ()) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || now () < it.value ().deadline_ms) return false;
  finish (it.value (), Status::Timeout, QStringLiteral ("feedback_timeout"));
  it.value ().timed_out = true;
  if (it.value ().dispatched)
    {
      unconfirmed_latch_ = true;
      last_timed_out_request_id_ = it.value ().result.request_id;
    }
  return true;
}

void JtdxWebControl::arm_timer ()
{
  if (!shutdown_ && !pending_request_id_.isEmpty () && !test_clock_)
    expiry_timer_.start ();
}

void JtdxWebControl::on_timer ()
{
  expire ();
}

bool JtdxWebControl::rotate_epoch ()
{
  if (shutdown_ || !pending_request_id_.isEmpty ()) return false;
  if (!unconfirmed_latch_)
    {
      if (!records_.isEmpty ()) mark_operations_changed ();
      records_.clear ();
    }
  epoch_ = QUuid::createUuid ().toString (QUuid::WithoutBraces);
  return true;
}

void JtdxWebControl::shutdown ()
{
  if (shutdown_) return;
  invalidate_server_epoch (QStringLiteral ("shutdown"));
  shutdown_ = true;
  server_epoch_bound_ = false;
  expiry_timer_.stop ();
}

void JtdxWebControl::set_clock_for_test (qint64 monotonic_ms)
{
  test_clock_ = true;
  test_now_ms_ = qMax<qint64> (0, monotonic_ms);
}

void JtdxWebControl::advance_clock_for_test (qint64 elapsed_ms)
{
  if (!test_clock_) set_clock_for_test (0);
  test_now_ms_ = qMax<qint64> (0, test_now_ms_ + elapsed_ms);
}
