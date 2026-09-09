#include "JtdxWebControl.hpp"

#include <QCoreApplication>
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
  return operation == Operation::Frequency ? QStringLiteral ("frequency")
                                            : QStringLiteral ("select-dx");
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

QString JtdxWebControl::canonical_payload (Request const& request)
{
  if (request.operation == Operation::Frequency)
    return QStringLiteral ("frequency:") + QString::number (request.frequency_hz);
  QString const call = request.dx_call.trimmed ().toUpper ();
  QString const grid = request.dx_grid.trimmed ().toUpper ();
  return QStringLiteral ("select-dx:") + QString::number (call.size ()) + QStringLiteral (":") + call
      + QString::number (grid.size ()) + QStringLiteral (":") + grid;
}

bool JtdxWebControl::safe_to_dispatch (SafetySnapshot const& safety, QString * reason)
{
  if (!safety.known || !safety.fresh) *reason = QStringLiteral ("safety_unknown_or_stale");
  else if (safety.transmitting) *reason = QStringLiteral ("transmitting");
  else if (safety.ptt) *reason = QStringLiteral ("ptt_active");
  else if (safety.tx_enabled) *reason = QStringLiteral ("tx_enabled");
  else if (safety.watchdog_timeout) *reason = QStringLiteral ("watchdog_timeout");
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

void JtdxWebControl::set_observed_state (ObservedState state)
{
  observed_ = std::move (state);
}

void JtdxWebControl::set_observation_provider (ObservationProvider provider)
{
  observation_provider_ = std::move (provider);
}

void JtdxWebControl::set_frequency_dispatcher (DispatchHandler handler)
{
  frequency_dispatcher_ = std::move (handler);
}

void JtdxWebControl::set_select_dx_dispatcher (DispatchHandler handler)
{
  select_dx_dispatcher_ = std::move (handler);
}

JtdxWebControl::Result JtdxWebControl::reject (Request const& request, QString reason,
                                               int http_status) const
{
  Result result;
  result.request_id = request.request_id;
  result.operation = request.operation;
  result.status = Status::Rejected;
  result.reason = std::move (reason);
  result.server_epoch = epoch_;
  result.http_status = http_status;
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
}

JtdxWebControl::Result JtdxWebControl::submit (Request request)
{
  if (shutdown_) return reject (request, QStringLiteral ("shutdown"), 409);
  request.request_id = normalize_request_id (request.request_id);
  if (request.request_id.isEmpty ()) return reject (request, QStringLiteral ("invalid_request_id"), 400);
  if (request.server_epoch != epoch_) return reject (request, QStringLiteral ("epoch_conflict"), 409);
  if (request.operation == Operation::Frequency)
    {
      if (request.frequency_hz <= 0) return reject (request, QStringLiteral ("invalid_frequency"), 400);
    }
  else
    {
      request.dx_call = request.dx_call.trimmed ().toUpper ();
      request.dx_grid = request.dx_grid.trimmed ().toUpper ();
      if (!printable_ascii (request.dx_call, max_target_length)
          || (!request.dx_grid.isEmpty () && !printable_ascii (request.dx_grid, max_grid_length)))
        return reject (request, QStringLiteral ("invalid_normalized_dx"), 400);
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
        return existing.value ().result;
      return reject (request, QStringLiteral ("request_id_conflict"), 409);
    }
  if (records_.size () >= hard_record_limit_)
    return reject (request, QStringLiteral ("record_limit"), 429);
  if (!pending_request_id_.isEmpty ())
    return reject (request, QStringLiteral ("busy"), 409);

  ObservedState current;
  try
    {
      current = observation ();
    }
  catch (...)
    {
      return reject (request, QStringLiteral ("observation_unavailable"), 503);
    }
  if (shutdown_ || epoch_ != request_epoch)
    return reject (request, QStringLiteral ("epoch_changed"), 409);
  if (current.state_revision != request.state_revision)
    return reject (request, QStringLiteral ("state_revision_conflict"), 409);
  QString safety_reason;
  if (!safe_to_dispatch (current.safety, &safety_reason))
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
  records_.insert (request.request_id, record);
  auto inserted = records_.find (request.request_id);
  inserted.value ().result.received_ms = record.received_ms;
  inserted.value ().result.deadline_ms = record.deadline_ms;
  inserted.value ().result.initial_state_revision = request.state_revision;
  inserted.value ().result.snapshot = current;
  inserted.value ().result.status_history.append (Status::Received);

  // 再读一次安全状态，防止 accepted 快照在 dispatch 前已失效。
  try
    {
      current = observation ();
    }
  catch (...)
    {
      auto failed = records_.find (request.request_id);
      if (epoch_ == request_epoch && failed != records_.end () && failed.value ().result.server_epoch == request_epoch
          && failed.value ().result.status == Status::Received)
        finish (failed.value (), Status::Rejected,
                                              QStringLiteral ("observation_unavailable"));
      auto result_after_failure = records_.constFind (request.request_id);
      return result_after_failure == records_.constEnd () ? reject (request, QStringLiteral ("epoch_changed"), 409)
                                                            : result_after_failure.value ().result;
    }
  auto stored_it = records_.find (request.request_id);
  if (shutdown_ || epoch_ != request_epoch || stored_it == records_.end ())
    {
      auto gone = records_.constFind (request.request_id);
      return gone == records_.constEnd () ? reject (request, QStringLiteral ("epoch_changed"), 409)
                                           : gone.value ().result;
    }
  Record& stored = stored_it.value ();
  stored.result.snapshot = current;
  if (current.state_revision != request.state_revision
      || !safe_to_dispatch (current.safety, &safety_reason))
    {
      finish (stored, Status::Rejected,
              current.state_revision != request.state_revision ? QStringLiteral ("state_changed_before_dispatch")
                                                                : safety_reason);
      return stored.result;
    }
  stored.baseline_generation = request.operation == Operation::Frequency
      ? current.frequency_generation : current.dx_generation;
  stored.baseline_state_revision = current.state_revision;
  bool const already_matches = request.operation == Operation::Frequency
      ? (current.frequency_known && current.actual_frequency_hz == request.frequency_hz)
      : (current.dx_known && current.dx_call.trimmed ().toUpper () == request.dx_call
         && current.dx_grid.trimmed ().toUpper () == request.dx_grid);
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
  DispatchHandler handler = request.operation == Operation::Frequency
      ? frequency_dispatcher_ : select_dx_dispatcher_;
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
  if (epoch_ != request_epoch)
    return reject (request, QStringLiteral ("epoch_changed"), 409);
  auto completed = records_.constFind (request.request_id);
  return completed == records_.constEnd () ? reject (request, QStringLiteral ("record_lost"))
                                             : completed.value ().result;
}

bool JtdxWebControl::feedback_frequency (QString const& request_id, QString const& server_epoch,
                                         quint64 generation, qint64 actual_frequency_hz,
                                         quint64 state_revision)
{
  if (pending_request_id_.isEmpty ())
    {
      if (server_epoch == epoch_ && request_id == last_timed_out_request_id_)
        {
          auto late = records_.find (last_timed_out_request_id_);
          if (late != records_.end () && late.value ().timed_out)
            late.value ().result.reason = QStringLiteral ("feedback_timeout/late_feedback_unknown");
        }
      return false;
    }
  if (request_id != pending_request_id_ || server_epoch != epoch_) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || it.value ().result.operation != Operation::Frequency) return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
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
                                         quint64 state_revision)
{
  if (pending_request_id_.isEmpty ())
    {
      if (server_epoch == epoch_ && request_id == last_timed_out_request_id_)
        {
          auto late = records_.find (last_timed_out_request_id_);
          if (late != records_.end () && late.value ().timed_out)
            late.value ().result.reason = QStringLiteral ("feedback_timeout/late_feedback_unknown");
        }
      return false;
    }
  if (request_id != pending_request_id_ || server_epoch != epoch_) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || it.value ().result.operation != Operation::SelectDx) return false;
  Record& record = it.value ();
  if (now () >= record.deadline_ms)
    {
      finish (record, Status::Timeout, QStringLiteral ("feedback_timeout"));
      record.timed_out = true;
      last_timed_out_request_id_ = request_id;
      return false;
    }
  if (record.result.server_epoch != epoch_ || generation <= record.baseline_generation
      || state_revision <= record.baseline_state_revision) return false;
  dx_call = dx_call.trimmed ().toUpper ();
  dx_grid = dx_grid.trimmed ().toUpper ();
  if (dx_call != record.dx_call || dx_grid != record.dx_grid) return false;
  record.result.snapshot = observed_;
  record.result.snapshot.dx_known = true;
  record.result.snapshot.dx_call = dx_call;
  record.result.snapshot.dx_grid = dx_grid;
  record.result.snapshot.dx_generation = generation;
  record.result.snapshot.state_revision = state_revision;
  finish (record, Status::Completed, QStringLiteral ("feedback_matched"), generation);
  return true;
}

bool JtdxWebControl::expire ()
{
  if (pending_request_id_.isEmpty ()) return false;
  auto it = records_.find (pending_request_id_);
  if (it == records_.end () || now () < it.value ().deadline_ms) return false;
  finish (it.value (), Status::Timeout, QStringLiteral ("feedback_timeout"));
  it.value ().timed_out = true;
  last_timed_out_request_id_ = it.value ().result.request_id;
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
  records_.clear ();
  epoch_ = QUuid::createUuid ().toString (QUuid::WithoutBraces);
  return true;
}

void JtdxWebControl::shutdown ()
{
  if (shutdown_) return;
  shutdown_ = true;
  if (!pending_request_id_.isEmpty ())
    {
      auto it = records_.find (pending_request_id_);
      if (it != records_.end ()) finish (it.value (), Status::Rejected, QStringLiteral ("shutdown"));
    }
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
