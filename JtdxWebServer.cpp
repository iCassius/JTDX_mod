#include "JtdxWebServer.hpp"

#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPointer>
#include <QTimer>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <limits>
#include <cmath>

namespace {
constexpr int header_timeout_ms = 5000;
constexpr int publish_interval_ms = 1000;
constexpr qint64 snapshot_interval_ms = 2000;
constexpr qint64 heartbeat_interval_ms = 10000;
constexpr int response_drain_timeout_ms = 2000;

QByteArray status_reason (int status)
{
  switch (status)
    {
    case 200: return QByteArrayLiteral ("OK");
    case 202: return QByteArrayLiteral ("Accepted");
    case 400: return QByteArrayLiteral ("Bad Request");
    case 409: return QByteArrayLiteral ("Conflict");
    case 401: return QByteArrayLiteral ("Unauthorized");
    case 403: return QByteArrayLiteral ("Forbidden");
    case 404: return QByteArrayLiteral ("Not Found");
    case 405: return QByteArrayLiteral ("Method Not Allowed");
    case 408: return QByteArrayLiteral ("Request Timeout");
    case 413: return QByteArrayLiteral ("Payload Too Large");
    case 429: return QByteArrayLiteral ("Too Many Requests");
    case 502: return QByteArrayLiteral ("Bad Gateway");
    case 504: return QByteArrayLiteral ("Gateway Timeout");
    case 431: return QByteArrayLiteral ("Request Header Fields Too Large");
    case 500: return QByteArrayLiteral ("Internal Server Error");
    case 503: return QByteArrayLiteral ("Service Unavailable");
    default: return QByteArrayLiteral ("Error");
    }
}

QString new_epoch ()
{
  return QUuid::createUuid ().toString (QUuid::WithoutBraces);
}
}

struct JtdxWebServer::Client
{
  QTcpSocket * socket {nullptr};
  QTimer * header_timer {nullptr};
  QTimer * drain_timer {nullptr};
  QByteArray request;
  qint64 expected_request_size {-1};
  QByteArray pending;
  bool sse {false};
  bool draining {false};
};

JtdxWebServer::JtdxWebServer (JtdxWebState * state, QObject * parent)
  : QObject {parent}
  , state_ {state}
{
  activity_clock_.start ();
  publish_timer_ = new QTimer {this};
  publish_timer_->setInterval (publish_interval_ms);
  snapshot_push_timer_ = new QTimer {this};
  snapshot_push_timer_->setSingleShot (true);
  snapshot_push_timer_->setInterval (20);
  connect (&server_, &QTcpServer::newConnection, this, &JtdxWebServer::accept_connections);
  connect (publish_timer_, &QTimer::timeout, this, &JtdxWebServer::on_publish_timer);
  connect (snapshot_push_timer_, &QTimer::timeout, this, [this] { broadcast_snapshot (true); });
  if (state_)
    {
      connect (state_, &QObject::destroyed, this, [this] { stop (); });
      connect (state_, &JtdxWebState::state_changed, this,
               [this] (quint64) { schedule_snapshot_push (); });
    }
}

JtdxWebServer::~JtdxWebServer ()
{
  stop ();
}

bool JtdxWebServer::validate_configuration (Configuration const& configuration,
                                             QString * error) const
{
  if (configuration.bind_address.isNull ()
      || configuration.bind_address == QHostAddress::Any
      || configuration.bind_address == QHostAddress::AnyIPv4
      || configuration.bind_address == QHostAddress::AnyIPv6)
    {
      if (error) *error = QStringLiteral ("a concrete listen address is required");
      return false;
    }
  if (configuration.automatic_port)
    {
      if (configuration.port != 0)
        {
          if (error) *error = QStringLiteral ("automatic port requires port=0");
          return false;
        }
    }
  else if (configuration.port < 1024)
    {
      if (error) *error = QStringLiteral ("manual port must be 1024..65535");
      return false;
    }
  if (!configuration.automatic_port && configuration.udp_ports.contains (configuration.port))
    {
      if (error) *error = QStringLiteral ("web port is reserved by UDP configuration");
      return false;
    }
  return true;
}

bool JtdxWebServer::listen_on (QHostAddress const& address, quint16 port)
{
  if (!server_.listen (address, port)) return false;
  actual_address_ = server_.serverAddress ();
  actual_port_ = server_.serverPort ();
  web_server_state_ = QStringLiteral ("listening");
  last_error_.clear ();
  last_published_revision_ = state_ ? state_->revision () : 0;
  last_published_operations_revision_ = control_ ? control_->operation_revision () : 0;
  last_snapshot_ms_ = -1;
  last_heartbeat_ms_ = -1;
  publish_timer_->start ();
  return true;
}

bool JtdxWebServer::choose_and_listen (Configuration const& configuration)
{
  if (!configuration.automatic_port)
    return listen_on (configuration.bind_address, configuration.port);

  for (quint32 candidate = automatic_port_first; candidate <= automatic_port_last; ++candidate)
    {
      quint16 const port = static_cast<quint16> (candidate);
      if (configuration.udp_ports.contains (port)) continue;
      if (listen_on (configuration.bind_address, port)) return true;
    }
  return false;
}

bool JtdxWebServer::start (Configuration configuration)
{
  if (is_listening ()) return true;
  if (!state_)
    {
      last_error_ = QStringLiteral ("state object is required");
      web_server_state_ = QStringLiteral ("error");
      return false;
    }
  QString error;
  if (!validate_configuration (configuration, &error))
    {
      last_error_ = error;
      web_server_state_ = QStringLiteral ("error");
      return false;
    }
  configuration_ = configuration;
  server_epoch_ = new_epoch ();
  if (choose_and_listen (configuration))
    {
      Q_EMIT lifecycle_changed (server_epoch_, true);
      return true;
    }
  last_error_ = server_.errorString ();
  if (last_error_.isEmpty ()) last_error_ = QStringLiteral ("unable to bind TCP port");
  web_server_state_ = QStringLiteral ("error");
  server_.close ();
  actual_address_ = QHostAddress {};
  actual_port_ = 0;
  Q_EMIT lifecycle_changed (server_epoch_, false);
  return false;
}

void JtdxWebServer::stop ()
{
  if (publish_timer_) publish_timer_->stop ();
  if (snapshot_push_timer_) snapshot_push_timer_->stop ();
  server_.close ();
  while (server_.hasPendingConnections ())
    {
      QTcpSocket * socket = server_.nextPendingConnection ();
      if (socket)
        {
          socket->abort ();
          socket->deleteLater ();
        }
    }
  QList<QTcpSocket *> sockets = clients_.keys ();
  for (QTcpSocket * socket : sockets) close_client (socket);
  actual_address_ = QHostAddress {};
  actual_port_ = 0;
  web_server_state_ = QStringLiteral ("stopped");
  Q_EMIT lifecycle_changed (server_epoch_, false);
}

bool JtdxWebServer::is_listening () const
{
  return server_.isListening ();
}

quint16 JtdxWebServer::actual_port () const
{
  return actual_port_;
}

QHostAddress JtdxWebServer::actual_address () const
{
  return actual_address_;
}

QString JtdxWebServer::url () const
{
  if (!is_listening ()) return {};
  QString address = actual_address_.toString ();
  if (actual_address_.protocol () == QAbstractSocket::IPv6Protocol)
    address = QStringLiteral ("[") + address + QStringLiteral ("]");
  return QStringLiteral ("http://") + address + QStringLiteral (":")
       + QString::number (actual_port_);
}

QString JtdxWebServer::last_error () const
{
  return last_error_;
}

QString JtdxWebServer::server_epoch () const
{
  return server_epoch_;
}

QString JtdxWebServer::web_server_state () const
{
  return web_server_state_;
}

int JtdxWebServer::active_connection_count () const
{
  return clients_.size ();
}

void JtdxWebServer::set_control (JtdxWebControl * control)
{
  QObject::disconnect (control_destroyed_connection_);
  QObject::disconnect (control_operations_connection_);
  control_ = control;
  ++control_generation_;
  // 协调器替换也要让现有 SSE 客户端获得一次新的有界投影。
  last_published_operations_revision_ = std::numeric_limits<quint64>::max ();
  if (control_)
    {
      control_destroyed_connection_ = connect (control_, &QObject::destroyed, this, [this] {
        ++control_generation_;
        last_published_operations_revision_ = std::numeric_limits<quint64>::max ();
      });
      control_operations_connection_ = connect (control_, &JtdxWebControl::operations_changed, this,
                                                  [this] (quint64) { schedule_snapshot_push (); });
    }
  schedule_snapshot_push ();
}

void JtdxWebServer::accept_connections ()
{
  while (server_.hasPendingConnections ())
    {
      QTcpSocket * socket = server_.nextPendingConnection ();
      if (clients_.size () >= max_connections)
        {
          socket->write (http_response (503, QByteArrayLiteral ("Service Unavailable"),
                                        QByteArrayLiteral ("text/plain; charset=utf-8"),
                                        QByteArrayLiteral ("connection limit\n")));
          connect (socket, &QTcpSocket::bytesWritten, socket, [socket] {
            if (socket->bytesToWrite () == 0) socket->disconnectFromHost ();
          });
          connect (socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
          continue;
        }
      auto * client = new Client;
      client->socket = socket;
      client->header_timer = new QTimer {this};
      client->header_timer->setSingleShot (true);
      client->header_timer->setInterval (header_timeout_ms);
      client->drain_timer = new QTimer {this};
      client->drain_timer->setSingleShot (true);
      client->drain_timer->setInterval (response_drain_timeout_ms);
      clients_.insert (socket, client);
      socket->setReadBufferSize (max_header_bytes);
      // Keep the kernel-side send queue bounded as well as the application
      // queue, so an unread SSE peer cannot hide unbounded backpressure in
      // the platform socket buffer.
      socket->setSocketOption (QAbstractSocket::SendBufferSizeSocketOption, 16 * 1024);
      connect (socket, &QTcpSocket::readyRead, this, [this, socket] { on_ready_read (socket); });
      connect (socket, &QTcpSocket::disconnected, this, [this, socket] { on_disconnected (socket); });
      connect (socket, &QTcpSocket::bytesWritten, this, [this, socket] {
        Client * c = clients_.value (socket, nullptr);
        if (!c) return;
        if (c->sse) pump_sse (c);
        else if (c->draining && socket->bytesToWrite () == 0) remove_client (socket);
      });
      connect (client->header_timer, &QTimer::timeout, this, [this, socket] {
        on_header_timeout (socket);
      });
      connect (client->drain_timer, &QTimer::timeout, this, [this, socket] {
        if (clients_.contains (socket))
          {
            socket->abort ();
            remove_client (socket);
          }
      });
      client->header_timer->start ();
    }
}

void JtdxWebServer::on_header_timeout (QTcpSocket * socket)
{
  if (Client * client = clients_.value (socket, nullptr))
    {
      if (!client->sse && !client->draining)
        reject_connection (socket, 408, QByteArrayLiteral ("header timeout"));
    }
}

void JtdxWebServer::on_disconnected (QTcpSocket * socket)
{
  remove_client (socket);
}

void JtdxWebServer::remove_client (QTcpSocket * socket)
{
  Client * client = clients_.take (socket);
  if (!client) return;
  if (client->header_timer) client->header_timer->stop ();
  if (client->drain_timer) client->drain_timer->stop ();
  if (client->header_timer) client->header_timer->deleteLater ();
  if (client->drain_timer) client->drain_timer->deleteLater ();
  if (socket)
    {
      socket->disconnectFromHost ();
      socket->deleteLater ();
    }
  delete client;
}

void JtdxWebServer::close_client (QTcpSocket * socket)
{
  if (!socket) return;
  socket->abort ();
  remove_client (socket);
}

void JtdxWebServer::reject_connection (QTcpSocket * socket, int status, QByteArray const& reason)
{
  if (!socket) return;
  QByteArray body = reason + QByteArrayLiteral ("\n");
  send_http (socket, status, status_reason (status), QByteArrayLiteral ("text/plain; charset=utf-8"), body);
}

void JtdxWebServer::on_ready_read (QTcpSocket * socket)
{
  Client * client = clients_.value (socket, nullptr);
  if (!client) return;
  QByteArray bytes = socket->readAll ();
  if (client->draining)
    {
      if (!bytes.isEmpty ())
        {
          socket->abort ();
          remove_client (socket);
        }
      return;
    }
  if (client->sse)
    {
      // An SSE client has no request body or second request. Keep the receive
      // side bounded and isolate protocol violations to this connection.
      if (!bytes.isEmpty ()) close_client (socket);
      return;
    }
  client->request += bytes;
  if (client->request.size () > max_header_bytes + 4096 + 4)
    {
      reject_connection (socket, 431, QByteArrayLiteral ("header limit"));
      return;
    }
  int const header_end = client->request.indexOf (QByteArrayLiteral ("\r\n\r\n"));
  if (header_end < 0) return;
  if (header_end + 4 > max_header_bytes)
    {
      reject_connection (socket, 431, QByteArrayLiteral ("header limit"));
      return;
    }
  if (client->expected_request_size < 0)
    {
      QByteArray const header = client->request.left (header_end);
      QList<QByteArray> lines = header.split ('\n');
      int content_length_count = 0;
      qint64 content_length = 0;
      for (QByteArray line : lines)
        {
          if (line.endsWith ('\r')) line.chop (1);
          int const colon = line.indexOf (':');
          if (colon <= 0) continue;
          QByteArray const name = line.left (colon).trimmed ().toLower ();
          if (name != QByteArrayLiteral ("content-length")) continue;
          ++content_length_count;
          QByteArray const value = line.mid (colon + 1).trimmed ();
          if (value.isEmpty ())
            {
              reject_connection (socket, 400, QByteArrayLiteral ("invalid content-length"));
              return;
            }
          bool const exceeds_body_limit = value.size () > 4
              || (value.size () == 4 && value > QByteArrayLiteral ("4096"));
          if (exceeds_body_limit)
            {
              reject_connection (socket, 413, QByteArrayLiteral ("body limit"));
              return;
            }
          qint64 parsed = 0;
          for (char const digit : value)
            {
              if (digit < '0' || digit > '9')
                {
                  reject_connection (socket, 400, QByteArrayLiteral ("invalid content-length"));
                  return;
                }
              parsed = parsed * 10 + (digit - '0');
            }
          content_length = parsed;
        }
      if (content_length_count > 1)
        {
          reject_connection (socket, 400, QByteArrayLiteral ("duplicate content-length"));
          return;
        }
      client->expected_request_size = header_end + 4 + (content_length_count ? content_length : 0);
    }
  if (client->request.size () < client->expected_request_size) return;
  if (client->request.size () > client->expected_request_size)
    {
      reject_connection (socket, 400, QByteArrayLiteral ("request pipelining is not supported"));
      return;
    }
  QByteArray const header = client->request.left (header_end);
  for (int i = 0; i < header.size (); ++i)
    {
      if (header.at (i) == '\n' && (i == 0 || header.at (i - 1) != '\r'))
        {
          reject_connection (socket, 400, QByteArrayLiteral ("bare LF is unsupported"));
          return;
        }
      if (header.at (i) == '\r' && (i + 1 >= header.size () || header.at (i + 1) != '\n'))
        {
          reject_connection (socket, 400, QByteArrayLiteral ("invalid line ending"));
          return;
        }
    }
  if (client->header_timer) client->header_timer->stop ();
  QByteArray request = client->request;
  client->request.clear ();
  client->expected_request_size = -1;
  process_request (socket, request);
}

QByteArray JtdxWebServer::event_id () const
{
  QByteArray const material = QByteArray::number (state_ ? state_->revision () : 0)
      + QByteArrayLiteral ("|")
      + QByteArray::number (control_ ? control_->operation_revision () : 0)
      + QByteArrayLiteral ("|") + QByteArray::number (control_generation_);
  QByteArray const digest = QCryptographicHash::hash (material, QCryptographicHash::Sha256).toHex ().left (16);
  return server_epoch_.toUtf8 () + QByteArrayLiteral ("-") + digest;
}

QJsonObject JtdxWebServer::operation_result (JtdxWebControl::Result const& result) const
{
  QJsonObject output;
  output.insert (QStringLiteral ("request_id"), result.request_id);
  output.insert (QStringLiteral ("operation"), JtdxWebControl::operation_name (result.operation));
  output.insert (QStringLiteral ("status"), JtdxWebControl::status_name (result.status));
  output.insert (QStringLiteral ("reason"), result.reason);
  output.insert (QStringLiteral ("server_epoch"), result.server_epoch);
  output.insert (QStringLiteral ("received_ms"), result.received_ms);
  output.insert (QStringLiteral ("deadline_ms"), result.deadline_ms);
  output.insert (QStringLiteral ("completed_ms"), result.completed_ms < 0
                 ? QJsonValue {QJsonValue::Null} : QJsonValue {result.completed_ms});
  output.insert (QStringLiteral ("initial_state_revision"),
                 static_cast<qint64> (result.initial_state_revision));
  output.insert (QStringLiteral ("generation"), static_cast<qint64> (result.generation));

  QJsonObject readback;
  readback.insert (QStringLiteral ("confirmed"),
                   result.status == JtdxWebControl::Status::Completed);
  readback.insert (QStringLiteral ("state_revision"),
                   static_cast<qint64> (result.snapshot.state_revision));
  readback.insert (QStringLiteral ("safety_known"), result.snapshot.safety.known);
  readback.insert (QStringLiteral ("tx_enabled"), result.snapshot.safety.known
                   ? QJsonValue {result.snapshot.safety.tx_enabled} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("transmitting"), result.snapshot.safety.known
                   ? QJsonValue {result.snapshot.safety.transmitting} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("ptt"), result.snapshot.safety.known
                   ? QJsonValue {result.snapshot.safety.ptt} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("tune"), result.snapshot.safety.known
                   ? QJsonValue {result.snapshot.safety.tune} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("frequency_known"), result.snapshot.frequency_known);
  readback.insert (QStringLiteral ("frequency_hz"), result.snapshot.frequency_known
                   ? QJsonValue {QString::number (result.snapshot.actual_frequency_hz)}
                   : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("frequency_generation"),
                   static_cast<qint64> (result.snapshot.frequency_generation));
  readback.insert (QStringLiteral ("dx_known"), result.snapshot.dx_known);
  readback.insert (QStringLiteral ("dx_call"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_call} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_grid"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_grid} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_generation"),
                   static_cast<qint64> (result.snapshot.dx_generation));
  readback.insert (QStringLiteral ("dx_report"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_report} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_frequency_offset"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_frequency_offset} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_time"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_time} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_selection_source"), result.snapshot.dx_known
                   ? QJsonValue {result.snapshot.dx_selection_source} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("dx_source_decode_id"), result.snapshot.dx_known && result.snapshot.dx_source_decode_id != 0
                   ? QJsonValue {static_cast<qint64> (result.snapshot.dx_source_decode_id)} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("business_generation"), static_cast<qint64> (result.snapshot.business_generation));
  readback.insert (QStringLiteral ("auto_sequence_enabled"), result.snapshot.business_state_known
                   ? QJsonValue {result.snapshot.auto_sequence_enabled} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("cq_state"), result.snapshot.business_state_known
                   ? QJsonValue {result.snapshot.cq_state} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_state_known"), result.snapshot.radio_state_known);
  readback.insert (QStringLiteral ("radio_multi_decode"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_multi_decode} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_agc_compensation"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_agc_compensation} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_narrow"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_narrow} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_sync"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_sync} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_skip_tx1"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_skip_tx1} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_current_tx_index"), result.snapshot.radio_state_known ? QJsonValue {result.snapshot.radio_current_tx_index} : QJsonValue {QJsonValue::Null});
  readback.insert (QStringLiteral ("radio_tx_messages"), QJsonArray::fromStringList (result.snapshot.radio_tx_messages));
  readback.insert (QStringLiteral ("radio_log_dialog_open"), result.snapshot.radio_log_dialog_open);
  readback.insert (QStringLiteral ("radio_qso_draft"), result.snapshot.radio_qso_draft);
  readback.insert (QStringLiteral ("radio_qso_generation"), static_cast<qint64> (result.snapshot.radio_qso_generation));
  output.insert (QStringLiteral ("readback"), readback);
  return output;
}

QJsonObject JtdxWebServer::operations_snapshot () const
{
  QJsonObject output;
  output.insert (QStringLiteral ("server_epoch"), server_epoch_);
  output.insert (QStringLiteral ("operation_revision"),
                 static_cast<qint64> (control_ ? control_->operation_revision () : 0));
  QJsonArray operations;
  if (control_)
    for (auto const& result : control_->operation_results ())
      if (result.server_epoch == server_epoch_)
        operations.append (operation_result (result));
  output.insert (QStringLiteral ("operations"), operations);
  return output;
}

QJsonObject JtdxWebServer::state_snapshot () const
{
  QJsonObject snapshot = state_ ? state_->json_snapshot () : QJsonObject {};
  snapshot.insert (QStringLiteral ("server_epoch"), server_epoch_);
  QJsonObject const operations = operations_snapshot ();
  snapshot.insert (QStringLiteral ("operation_revision"), operations.value (QStringLiteral ("operation_revision")));
  snapshot.insert (QStringLiteral ("operations"), operations.value (QStringLiteral ("operations")));
  return snapshot;
}

QByteArray JtdxWebServer::json_response (QJsonObject const& object) const
{
  return QJsonDocument {object}.toJson (QJsonDocument::Compact);
}

QJsonObject JtdxWebServer::control_response (JtdxWebControl::Result const& result) const
{
  QJsonObject response = operation_result (result);
  response.insert (QStringLiteral ("state_revision"), static_cast<qint64> (result.snapshot.state_revision));
  if (result.snapshot.frequency_known)
    response.insert (QStringLiteral ("frequency_hz"), QString::number (result.snapshot.actual_frequency_hz));
  else
    response.insert (QStringLiteral ("frequency_hz"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("current_state"), state_snapshot ());
  return response;
}

QJsonObject JtdxWebServer::control_error_response (int status, QString reason, QString request_id,
                                                   JtdxWebControl::Operation operation) const
{
  QJsonObject response;
  response.insert (QStringLiteral ("request_id"), request_id.isEmpty ()
                   ? QUuid::createUuid ().toString (QUuid::WithoutBraces) : request_id);
  response.insert (QStringLiteral ("operation"), JtdxWebControl::operation_name (operation));
  response.insert (QStringLiteral ("server_epoch"), server_epoch_);
  response.insert (QStringLiteral ("status"), QStringLiteral ("rejected"));
  response.insert (QStringLiteral ("reason"), std::move (reason));
  response.insert (QStringLiteral ("received_ms"), activity_clock_.elapsed ());
  response.insert (QStringLiteral ("deadline_ms"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("completed_ms"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("state_revision"), static_cast<qint64> (state_ ? state_->revision () : 0));
  response.insert (QStringLiteral ("frequency_hz"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("dx_call"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("dx_grid"), QJsonValue {QJsonValue::Null});
  response.insert (QStringLiteral ("current_state"), state_snapshot ());
  Q_UNUSED (status);
  return response;
}

QByteArray JtdxWebServer::http_response (int status, QByteArray const& reason,
                                         QByteArray const& content_type,
                                         QByteArray const& body, bool close) const
{
  QByteArray response = QByteArrayLiteral ("HTTP/1.1 ") + QByteArray::number (status)
      + QByteArrayLiteral (" ") + reason + QByteArrayLiteral ("\r\nContent-Type: ")
      + content_type + QByteArrayLiteral ("\r\nContent-Length: ")
      + QByteArray::number (body.size ()) + QByteArrayLiteral ("\r\nCache-Control: no-store\r\n")
      + QByteArrayLiteral ("X-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; frame-ancestors 'none'\r\nReferrer-Policy: no-referrer\r\n")
      + (close ? QByteArrayLiteral ("Connection: close\r\n") : QByteArrayLiteral ("Connection: keep-alive\r\n"))
      + QByteArrayLiteral ("\r\n") + body;
  return response;
}

QByteArray web_resource (QByteArray const& path, QByteArray * content_type)
{
  QString resource;
  if (path == QByteArrayLiteral ("/")) resource = QStringLiteral (":/web-ui/index.html");
  else if (path == QByteArrayLiteral ("/style.css")) resource = QStringLiteral (":/web-ui/style.css");
  else if (path == QByteArrayLiteral ("/app.js")) resource = QStringLiteral (":/web-ui/app.js");
  else return {};
  QFile file {resource};
  if (!file.open (QIODevice::ReadOnly)) return {};
  if (content_type)
    *content_type = path.endsWith (QByteArrayLiteral (".css"))
      ? QByteArrayLiteral ("text/css; charset=utf-8")
      : path.endsWith (QByteArrayLiteral (".js"))
        ? QByteArrayLiteral ("text/javascript; charset=utf-8")
        : QByteArrayLiteral ("text/html; charset=utf-8");
  return file.readAll ();
}

void JtdxWebServer::send_http (QTcpSocket * socket, int status, QByteArray const& reason,
                               QByteArray const& content_type, QByteArray const& body)
{
  Client * client = clients_.value (socket, nullptr);
  if (!client || !socket) return;
  client->draining = true;
  if (socket->write (http_response (status, reason, content_type, body)) < 0)
    {
      socket->abort ();
      remove_client (socket);
      return;
    }
  if (client->drain_timer) client->drain_timer->start ();
  if (socket->bytesToWrite () == 0) remove_client (socket);
}

QByteArray JtdxWebServer::sse_event (QByteArray const& name, QByteArray const& id,
                                     QByteArray const& data) const
{
  return QByteArrayLiteral ("event: ") + name + QByteArrayLiteral ("\n")
       + QByteArrayLiteral ("id: ") + id + QByteArrayLiteral ("\n")
       + QByteArrayLiteral ("data: ") + data + QByteArrayLiteral ("\n\n");
}

void JtdxWebServer::send_sse (Client * client, QByteArray const& payload)
{
  if (!client || !client->socket || !clients_.contains (client->socket)) return;
  // The OS send buffer is intentionally 16 KiB.  If a peer has already left
  // data queued there while our application queue is non-empty, it is a
  // non-reading peer; evict it before another snapshot can grow the queue.
  constexpr qint64 unread_socket_limit = 16 * 1024;
  if (payload.size () > max_sse_event_bytes
      || client->pending.size () + payload.size () > max_sse_pending_bytes
      || client->socket->bytesToWrite () + client->pending.size () + payload.size () > max_sse_pending_bytes
      || (!client->pending.isEmpty () && client->socket->bytesToWrite () >= unread_socket_limit))
    {
      close_client (client->socket);
      return;
    }
  client->pending += payload;
  pump_sse (client);
  if (client->pending.isEmpty ())
    client->drain_timer->stop ();
  else
    client->drain_timer->start (response_drain_timeout_ms);
}

void JtdxWebServer::pump_sse (Client * client)
{
  if (!client || !client->socket || !clients_.contains (client->socket)) return;
  if (client->socket->bytesToWrite () >= max_sse_pending_bytes)
    {
      close_client (client->socket);
      return;
    }
  if (client->pending.isEmpty ()) return;
  qint64 const chunk = qMin<qint64> (64 * 1024, client->pending.size ());
  qint64 const written = client->socket->write (client->pending.constData (), chunk);
  if (written < 0)
    {
      close_client (client->socket);
      return;
    }
  client->pending.remove (0, static_cast<int> (written));
  if (client->pending.isEmpty () && client->drain_timer)
    client->drain_timer->stop ();
}

void JtdxWebServer::send_initial_sse (Client * client, QByteArray const& last_event_id)
{
  QByteArray const headers = QByteArrayLiteral ("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n\r\n");
  if (client->socket->write (headers) < 0)
    {
      close_client (client->socket);
      return;
    }
  client->sse = true;
  QTcpSocket * const socket = client->socket;
  if (!last_event_id.isEmpty ())
    {
      QJsonObject required;
      required.insert (QStringLiteral ("reason"), QStringLiteral ("history_not_retained"));
      required.insert (QStringLiteral ("server_epoch"), server_epoch_);
      required.insert (QStringLiteral ("state_revision"), static_cast<qint64> (state_ ? state_->revision () : 0));
      send_sse (client, sse_event (QByteArrayLiteral ("resync_required"), event_id (), json_response (required)));
    }
  if (clients_.contains (socket))
    send_sse (clients_.value (socket), sse_event (QByteArrayLiteral ("snapshot"), event_id (), json_response (state_snapshot ())));
}

void JtdxWebServer::process_request (QTcpSocket * socket, QByteArray const& request)
{
  Client * client = clients_.value (socket, nullptr);
  if (!client) return;
  int const header_end = request.indexOf (QByteArrayLiteral ("\r\n\r\n"));
  if (header_end < 0)
    {
      reject_connection (socket, 400, QByteArrayLiteral ("invalid request"));
      return;
    }
  QByteArray const body = request.mid (header_end + 4);
  QList<QByteArray> lines = request.left (header_end).split ('\n');
  if (lines.isEmpty ())
    {
      reject_connection (socket, 400, QByteArrayLiteral ("empty request"));
      return;
    }
  QByteArray request_line = lines.takeFirst ();
  if (request_line.endsWith ('\r')) request_line.chop (1);
  QList<QByteArray> request_parts = request_line.split (' ');
  if (request_parts.size () != 3 || request_parts.at (2) != QByteArrayLiteral ("HTTP/1.1"))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("only HTTP/1.1 is supported"));
      return;
    }
  QByteArray const method = request_parts.at (0);
  QByteArray const target = request_parts.at (1);
  if (target.size () > 4096 || !target.startsWith ('/') || target.contains ('?') || target.contains ('#'))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("invalid target"));
      return;
    }

  QHash<QByteArray, QByteArray> headers;
  for (QByteArray line : lines)
    {
      if (line.endsWith ('\r')) line.chop (1);
      int const colon = line.indexOf (':');
      if (colon <= 0)
        {
          reject_connection (socket, 400, QByteArrayLiteral ("invalid header"));
          return;
        }
      QByteArray name = line.left (colon).toLower ();
      QByteArray value = line.mid (colon + 1).trimmed ();
      for (char ch : name)
        {
          bool const alpha = (ch >= 'a' && ch <= 'z');
          bool const digit = (ch >= '0' && ch <= '9');
          QByteArray const punctuation = QByteArrayLiteral ("!#$%&'*+-.^_`|~");
          if (!alpha && !digit && !punctuation.contains (ch))
            {
              reject_connection (socket, 400, QByteArrayLiteral ("invalid header name"));
              return;
            }
        }
      if (name.isEmpty () || headers.contains (name)
          || value.contains ('\r') || value.contains ('\n'))
        {
          reject_connection (socket, 400, QByteArrayLiteral ("duplicate or invalid header"));
          return;
        }
      headers.insert (name, value);
    }
  if (headers.contains (QByteArrayLiteral ("transfer-encoding"))
      || headers.contains (QByteArrayLiteral ("expect"))
      || headers.contains (QByteArrayLiteral ("upgrade")))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("transfer-encoding, expect or upgrade is unsupported"));
      return;
    }
  if (method != QByteArrayLiteral ("GET") && method != QByteArrayLiteral ("POST"))
    {
      reject_connection (socket, 405, QByteArrayLiteral ("GET and supported control POST only"));
      return;
    }
  if (method == QByteArrayLiteral ("POST")
      && headers.value (QByteArrayLiteral ("content-type")).toLower ()
           != QByteArrayLiteral ("application/json"))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("application/json is required"));
      return;
    }
  if (method == QByteArrayLiteral ("GET") && headers.contains (QByteArrayLiteral ("content-length")))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("GET must not contain a body"));
      return;
    }
  if (method == QByteArrayLiteral ("POST") && !headers.contains (QByteArrayLiteral ("content-length")))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("content-length is required"));
      return;
    }
  QByteArray const path = target;
  if (method == QByteArrayLiteral ("GET") && path == QByteArrayLiteral ("/"))
    {
      QByteArray content_type;
      QByteArray const body = web_resource (path, &content_type);
      if (body.isEmpty ())
        {
          reject_connection (socket, 500, QByteArrayLiteral ("web resource unavailable"));
          return;
        }
      send_http (socket, 200, QByteArrayLiteral ("OK"), content_type, body);
      return;
    }
  if (method == QByteArrayLiteral ("GET")
      && (path == QByteArrayLiteral ("/style.css") || path == QByteArrayLiteral ("/app.js")))
    {
      QByteArray content_type;
      QByteArray const body = web_resource (path, &content_type);
      if (body.isEmpty ())
        {
          reject_connection (socket, 500, QByteArrayLiteral ("web resource unavailable"));
          return;
        }
      send_http (socket, 200, QByteArrayLiteral ("OK"), content_type, body);
      return;
    }
  if (method == QByteArrayLiteral ("POST")
      && (path == QByteArrayLiteral ("/api/v1/control/start-cq")
          || path == QByteArrayLiteral ("/api/v1/control/start-auto-call")
          || path == QByteArrayLiteral ("/api/v1/control/stop-auto-call")))
    {
      auto const operation = path.endsWith (QByteArrayLiteral ("start-cq"))
          ? JtdxWebControl::Operation::StartCq
          : path.endsWith (QByteArrayLiteral ("start-auto-call"))
            ? JtdxWebControl::Operation::StartAutoCall : JtdxWebControl::Operation::StopAutoCall;
      bool const valid_utf8_body = !body.isEmpty () && body.size () <= 4096
          && QString::fromUtf8 (body).toUtf8 () == body;
      QJsonParseError parse_error {};
      QJsonDocument document;
      bool parsed_object = false;
      QString trusted_request_id;
      if (valid_utf8_body)
        {
          document = QJsonDocument::fromJson (body, &parse_error);
          parsed_object = parse_error.error == QJsonParseError::NoError && document.isObject ();
          if (parsed_object && document.object ().value (QStringLiteral ("request_id")).isString ())
            trusted_request_id = JtdxWebControl::normalize_request_id (
                document.object ().value (QStringLiteral ("request_id")).toString ());
        }
      auto reject_control = [&] (int status, QString reason) {
        send_http (socket, status, status_reason (status), QByteArrayLiteral ("application/json"),
                   json_response (control_error_response (status, std::move (reason),
                                                         trusted_request_id, operation)));
      };
      if (!control_ || !state_) { reject_control (409, QStringLiteral ("control_unavailable")); return; }
      if (!valid_utf8_body || !parsed_object) { reject_control (400, QStringLiteral ("invalid_json")); return; }
      QJsonObject const input = document.object ();
      static const QSet<QString> fields {QStringLiteral ("request_id"), QStringLiteral ("server_epoch"),
                                         QStringLiteral ("state_revision"), QStringLiteral ("confirm")};
      for (QString const& key : input.keys ())
        if (!fields.contains (key)) { reject_control (400, QStringLiteral ("unknown_json_field")); return; }
      if (trusted_request_id.isEmpty () || !input.value (QStringLiteral ("server_epoch")).isString ())
        { reject_control (400, QStringLiteral ("invalid_control_fields")); return; }
      JtdxWebControl::Request control_request;
      control_request.request_id = input.value (QStringLiteral ("request_id")).toString ();
      control_request.server_epoch = input.value (QStringLiteral ("server_epoch")).toString ();
      control_request.state_revision = 0;
      control_request.operation = operation;
      auto const result = control_->submit (std::move (control_request));
      send_http (socket, result.http_status, status_reason (result.http_status),
                 QByteArrayLiteral ("application/json"), json_response (control_response (result)));
      return;
    }
  if (method == QByteArrayLiteral ("POST") && path == QByteArrayLiteral ("/api/v1/control/radio"))
    {
      bool const valid_utf8_body = !body.isEmpty () && body.size () <= 4096
          && QString::fromUtf8 (body).toUtf8 () == body;
      QJsonParseError parse_error {};
      QJsonDocument document;
      bool parsed_object = false;
      QString trusted_request_id;
      if (valid_utf8_body)
        {
          document = QJsonDocument::fromJson (body, &parse_error);
          parsed_object = parse_error.error == QJsonParseError::NoError && document.isObject ();
          if (parsed_object && document.object ().value (QStringLiteral ("request_id")).isString ())
            trusted_request_id = JtdxWebControl::normalize_request_id (
                document.object ().value (QStringLiteral ("request_id")).toString ());
        }
      auto reject_control = [&] (int status, QString reason) {
        send_http (socket, status, status_reason (status), QByteArrayLiteral ("application/json"),
                   json_response (control_error_response (status, std::move (reason),
                                                         trusted_request_id, JtdxWebControl::Operation::Radio)));
      };
      if (!control_ || !state_) { reject_control (409, QStringLiteral ("control_unavailable")); return; }
      if (!valid_utf8_body || !parsed_object) { reject_control (400, QStringLiteral ("invalid_json")); return; }
      QJsonObject const input = document.object ();
      static const QSet<QString> fields {QStringLiteral ("request_id"), QStringLiteral ("server_epoch"),
                                         QStringLiteral ("state_revision"), QStringLiteral ("action"),
                                         QStringLiteral ("value"), QStringLiteral ("tx_index"),
                                         QStringLiteral ("text"), QStringLiteral ("confirm"),
                                         QStringLiteral ("qso")};
      for (QString const& key : input.keys ())
        if (!fields.contains (key)) { reject_control (400, QStringLiteral ("unknown_json_field")); return; }
      if (trusted_request_id.isEmpty () || !input.value (QStringLiteral ("server_epoch")).isString ()
          || !input.value (QStringLiteral ("action")).isString ())
        { reject_control (400, QStringLiteral ("invalid_control_fields")); return; }
      QString const action = input.value (QStringLiteral ("action")).toString ().trimmed ();
      int tx_index = input.value (QStringLiteral ("tx_index")).toInt (0);
      if (input.contains (QStringLiteral ("tx_index"))
          && (!input.value (QStringLiteral ("tx_index")).isDouble () || tx_index < 0 || tx_index > 6))
        { reject_control (400, QStringLiteral ("invalid_tx_index")); return; }
      if (input.contains (QStringLiteral ("value")) && !input.value (QStringLiteral ("value")).isBool ())
        { reject_control (400, QStringLiteral ("invalid_value")); return; }
      QString radio_text = input.value (QStringLiteral ("text")).toString ();
      if (radio_text.size () > 64 || (input.contains (QStringLiteral ("text")) && !input.value (QStringLiteral ("text")).isString ()))
        { reject_control (400, QStringLiteral ("invalid_text")); return; }
      QJsonObject qso;
      if (input.contains (QStringLiteral ("qso")))
        {
          if (!input.value (QStringLiteral ("qso")).isObject ())
            { reject_control (400, QStringLiteral ("invalid_qso")); return; }
          qso = input.value (QStringLiteral ("qso")).toObject ();
          static const QSet<QString> qso_fields {
            QStringLiteral ("call"), QStringLiteral ("grid"), QStringLiteral ("mode"),
            QStringLiteral ("report_sent"), QStringLiteral ("report_received"),
            QStringLiteral ("name"), QStringLiteral ("tx_power"), QStringLiteral ("comments"),
            QStringLiteral ("eqsl_comments"), QStringLiteral ("start"), QStringLiteral ("end"),
            QStringLiteral ("frequency_hz")};
          for (QString const& key : qso.keys ())
            if (!qso_fields.contains (key)) { reject_control (400, QStringLiteral ("unknown_qso_field")); return; }
          static const QSet<QString> qso_text_fields {
            QStringLiteral ("call"), QStringLiteral ("grid"), QStringLiteral ("mode"),
            QStringLiteral ("report_sent"), QStringLiteral ("report_received"),
            QStringLiteral ("name"), QStringLiteral ("tx_power"), QStringLiteral ("comments"),
            QStringLiteral ("eqsl_comments"), QStringLiteral ("start"), QStringLiteral ("end")};
          for (QString const& key : qso_text_fields)
            if (qso.contains (key) && (!qso.value (key).isString () || qso.value (key).toString ().size () > 256))
              { reject_control (400, QStringLiteral ("invalid_qso_text")); return; }
          if (qso.contains (QStringLiteral ("frequency_hz"))
              && (!qso.value (QStringLiteral ("frequency_hz")).isDouble ()
                  || qso.value (QStringLiteral ("frequency_hz")).toDouble () < 1
                  || qso.value (QStringLiteral ("frequency_hz")).toDouble () > 60000000))
            { reject_control (400, QStringLiteral ("invalid_qso_frequency")); return; }
        }
      if (action == QStringLiteral ("log-qso-confirm") && qso.isEmpty ())
        { reject_control (400, QStringLiteral ("qso_draft_required")); return; }
      JtdxWebControl::Request control_request;
      control_request.request_id = input.value (QStringLiteral ("request_id")).toString ();
      control_request.server_epoch = input.value (QStringLiteral ("server_epoch")).toString ();
      control_request.state_revision = 0;
      control_request.operation = JtdxWebControl::Operation::Radio;
      control_request.radio_action = action;
      control_request.radio_value = input.value (QStringLiteral ("value")).toBool (false);
      control_request.radio_index = tx_index;
      control_request.radio_text = radio_text;
      control_request.radio_qso = qso;
      auto const result = control_->submit (std::move (control_request));
      send_http (socket, result.http_status, status_reason (result.http_status),
                 QByteArrayLiteral ("application/json"), json_response (control_response (result)));
      return;
    }
  if (method == QByteArrayLiteral ("POST") && path == QByteArrayLiteral ("/api/v1/control/select-dx"))
    {
      bool const valid_utf8_body = !body.isEmpty () && body.size () <= 4096
          && QString::fromUtf8 (body).toUtf8 () == body;
      QJsonParseError parse_error {};
      QJsonDocument document;
      bool parsed_object = false;
      QString trusted_request_id;
      if (valid_utf8_body)
        {
          document = QJsonDocument::fromJson (body, &parse_error);
          parsed_object = parse_error.error == QJsonParseError::NoError && document.isObject ();
          if (parsed_object && document.object ().value (QStringLiteral ("request_id")).isString ())
            trusted_request_id = JtdxWebControl::normalize_request_id (
                document.object ().value (QStringLiteral ("request_id")).toString ());
        }
      auto reject_control = [&] (int status, QString reason, QString request_id = {}) {
        QString effective_request_id = request_id.isEmpty ()
            ? trusted_request_id : JtdxWebControl::normalize_request_id (request_id);
        send_http (socket, status, status_reason (status), QByteArrayLiteral ("application/json"),
                   json_response (control_error_response (status, std::move (reason),
                                                         std::move (effective_request_id),
                                                         JtdxWebControl::Operation::SelectDx)));
      };
      if (!control_ || !state_)
        {
          reject_control (409, QStringLiteral ("control_unavailable"));
          return;
        }
      if (!valid_utf8_body)
        {
          reject_control (400, QStringLiteral ("invalid_utf8_body"));
          return;
        }
      if (!parsed_object)
        {
          reject_control (400, QStringLiteral ("invalid_json"));
          return;
        }
      QJsonObject const input = document.object ();
      static const QSet<QString> fields {
        QStringLiteral ("request_id"), QStringLiteral ("server_epoch"),
        QStringLiteral ("state_revision"), QStringLiteral ("decode_id"),
        QStringLiteral ("call"), QStringLiteral ("grid")};
      for (QString const& key : input.keys ())
        if (!fields.contains (key))
          {
            reject_control (400, QStringLiteral ("unknown_json_field"));
            return;
          }
      if (!input.value (QStringLiteral ("request_id")).isString ()
          || !input.value (QStringLiteral ("server_epoch")).isString ()
          || (!input.value (QStringLiteral ("decode_id")).isDouble ()
              && (!input.value (QStringLiteral ("call")).isString ()
                  || !input.value (QStringLiteral ("grid")).isString ()))
          || trusted_request_id.isEmpty ())
        {
          reject_control (400, QStringLiteral ("invalid_control_fields"));
          return;
        }
      double const decode_id_number = input.value (QStringLiteral ("decode_id")).toDouble ();
      bool const from_decode = input.value (QStringLiteral ("decode_id")).isDouble ();
      if (from_decode && (!std::isfinite (decode_id_number) || decode_id_number <= 0
          || decode_id_number > 9007199254740991.0 || decode_id_number != std::floor (decode_id_number)))
        {
          reject_control (400, QStringLiteral ("invalid_control_number"));
          return;
        }
      JtdxWebState::DecodeSelection selection;
      if (from_decode && !state_->decode_selection (static_cast<quint64> (decode_id_number), &selection))
        {
          reject_control (409, QStringLiteral ("invalid_or_stale_decode"));
          return;
        }
      QString const dx_call = from_decode ? selection.call : input.value (QStringLiteral ("call")).toString ();
      QString const dx_grid = from_decode ? selection.grid : input.value (QStringLiteral ("grid")).toString ();
      auto const normalized = JtdxWebDx::normalize (dx_call, dx_grid);
      if (!normalized.valid)
        {
          reject_control (400, normalized.reason);
          return;
        }
      JtdxWebControl::Request control_request;
      control_request.request_id = input.value (QStringLiteral ("request_id")).toString ();
      control_request.server_epoch = input.value (QStringLiteral ("server_epoch")).toString ();
      control_request.state_revision = 0;
      control_request.operation = JtdxWebControl::Operation::SelectDx;
      control_request.dx_call = normalized.call;
      control_request.dx_grid = normalized.grid;
      control_request.dx_frequency_offset = from_decode ? selection.delta_frequency : 0;
      control_request.dx_time = from_decode ? selection.time : QString {};
      control_request.dx_selection_source = from_decode ? QStringLiteral ("decode") : QStringLiteral ("manual");
      control_request.dx_source_decode_id = from_decode ? selection.decode_id : 0;
      auto const result = control_->submit (std::move (control_request));
      send_http (socket, result.http_status, status_reason (result.http_status),
                 QByteArrayLiteral ("application/json"), json_response (control_response (result)));
      return;
    }
  if (method == QByteArrayLiteral ("POST") && path == QByteArrayLiteral ("/api/v1/control/frequency"))
    {
      bool const valid_utf8_body = !body.isEmpty () && body.size () <= 4096
          && QString::fromUtf8 (body).toUtf8 () == body;
      QJsonDocument document;
      QJsonParseError parse_error {};
      bool parsed_object = false;
      QString trusted_request_id;
      if (valid_utf8_body)
        {
          document = QJsonDocument::fromJson (body, &parse_error);
          parsed_object = parse_error.error == QJsonParseError::NoError && document.isObject ();
          if (parsed_object)
            {
              QJsonValue const candidate = document.object ().value (QStringLiteral ("request_id"));
              if (candidate.isString ())
                trusted_request_id = JtdxWebControl::normalize_request_id (candidate.toString ());
            }
        }
      auto reject_control = [&] (int status, QString reason, QString request_id = QString {}) {
        QString effective_request_id = request_id.isEmpty ()
            ? trusted_request_id : JtdxWebControl::normalize_request_id (request_id);
        send_http (socket, status, status_reason (status), QByteArrayLiteral ("application/json"),
                   json_response (control_error_response (status, std::move (reason),
                                                         std::move (effective_request_id))));
      };
      if (!control_)
        {
          reject_control (409, QStringLiteral ("control_unavailable"));
          return;
        }
      if (!frequency_validator_)
        {
          reject_control (409, QStringLiteral ("frequency_validator_unavailable"));
          return;
        }
      if (!valid_utf8_body)
        {
          reject_control (400, QStringLiteral ("invalid_utf8_body"));
          return;
        }
      if (!parsed_object)
        {
          reject_control (400, QStringLiteral ("invalid_json"));
          return;
        }
      QJsonObject const input = document.object ();
      static const QSet<QString> fields {
        QStringLiteral ("request_id"), QStringLiteral ("server_epoch"), QStringLiteral ("state_revision"),
        QStringLiteral ("frequency_hz")};
      for (QString const& key : input.keys ())
        if (!fields.contains (key))
          {
            reject_control (400, QStringLiteral ("unknown_json_field"));
            return;
          }
      if (!input.value (QStringLiteral ("request_id")).isString ()
          || !input.value (QStringLiteral ("server_epoch")).isString ()
          || !input.value (QStringLiteral ("frequency_hz")).isString ())
        {
          reject_control (400, QStringLiteral ("invalid_control_fields"));
          return;
        }
      if (trusted_request_id.isEmpty ())
        {
          reject_control (400, QStringLiteral ("invalid_request_id"));
          return;
        }
      QString const frequency_text = input.value (QStringLiteral ("frequency_hz")).toString ();
      JtdxWebFrequency::Result const parsed_frequency = frequency_validator_ (frequency_text);
      if (!parsed_frequency.valid)
        {
          reject_control (400, QStringLiteral ("invalid_frequency_hz"));
          return;
        }
      qint64 const frequency_hz = parsed_frequency.frequency_hz;
      JtdxWebControl::Request control_request;
      control_request.request_id = input.value (QStringLiteral ("request_id")).toString ();
      control_request.server_epoch = input.value (QStringLiteral ("server_epoch")).toString ();
      control_request.state_revision = 0;
      control_request.frequency_hz = frequency_hz;
      control_request.operation = JtdxWebControl::Operation::Frequency;
      auto const result = control_->submit (std::move (control_request));
      send_http (socket, result.http_status, status_reason (result.http_status),
                 QByteArrayLiteral ("application/json"), json_response (control_response (result)));
      return;
    }
  if (method == QByteArrayLiteral ("POST"))
    {
      reject_connection (socket, 404, QByteArrayLiteral ("not found"));
      return;
    }
  if (path == QByteArrayLiteral ("/healthz"))
    {
      QJsonObject health;
      health.insert (QStringLiteral ("status"), QStringLiteral ("ok"));
      health.insert (QStringLiteral ("web_server_state"), web_server_state_);
      health.insert (QStringLiteral ("address"), actual_address_.toString ());
      health.insert (QStringLiteral ("port"), actual_port_);
      health.insert (QStringLiteral ("server_epoch"), server_epoch_);
      QByteArray const body = json_response (health);
      send_http (socket, 200, QByteArrayLiteral ("OK"), QByteArrayLiteral ("application/json"), body);
      return;
    }
  if (path == QByteArrayLiteral ("/api/v1/state") || path == QByteArrayLiteral ("/api/v1/decodes"))
    {
      QJsonObject response = state_snapshot ();
      if (path == QByteArrayLiteral ("/api/v1/decodes"))
        {
          response.insert (QStringLiteral ("decodes"), response.value (QStringLiteral ("recent_decodes")));
        }
      QByteArray const body = json_response (response);
      send_http (socket, 200, QByteArrayLiteral ("OK"), QByteArrayLiteral ("application/json"), body);
      return;
    }
  if (path == QByteArrayLiteral ("/api/v1/events"))
    {
      QByteArray const last_event_id = headers.value (QByteArrayLiteral ("last-event-id"));
      if (last_event_id.size () > max_last_event_id_bytes || last_event_id.contains ('\r')
          || last_event_id.contains ('\n') || last_event_id.contains (' '))
        {
          reject_connection (socket, 400, QByteArrayLiteral ("invalid last-event-id"));
          return;
        }
      send_initial_sse (client, last_event_id);
      return;
    }
  reject_connection (socket, 404, QByteArrayLiteral ("not found"));
}

void JtdxWebServer::broadcast_snapshot (bool force)
{
  if (!is_listening ()) return;
  qint64 const now = activity_clock_.elapsed ();
  quint64 const revision = state_ ? state_->revision () : 0;
  if (!force && revision == last_published_revision_ && last_snapshot_ms_ >= 0
      && now - last_snapshot_ms_ < snapshot_interval_ms) return;
  QByteArray const payload = sse_event (QByteArrayLiteral ("snapshot"), event_id (), json_response (state_snapshot ()));
  QList<QTcpSocket *> sockets;
  for (Client * client : clients_.values ())
    if (client->sse) sockets.append (client->socket);
  for (QTcpSocket * socket : sockets)
    if (Client * client = clients_.value (socket, nullptr)) send_sse (client, payload);
  last_published_revision_ = revision;
  last_published_operations_revision_ = control_ ? control_->operation_revision () : 0;
  last_snapshot_ms_ = now;
}

void JtdxWebServer::schedule_snapshot_push ()
{
  if (snapshot_push_timer_ && !snapshot_push_timer_->isActive ())
    snapshot_push_timer_->start ();
}

void JtdxWebServer::on_publish_timer ()
{
  if (!is_listening ()) return;
  qint64 const now = activity_clock_.elapsed ();
  quint64 const revision = state_ ? state_->revision () : 0;
  quint64 const operations_revision = control_ ? control_->operation_revision () : 0;
  bool const operations_changed = operations_revision != last_published_operations_revision_;
  if (last_snapshot_ms_ < 0 || revision != last_published_revision_
      || operations_changed || now - last_snapshot_ms_ >= snapshot_interval_ms)
    broadcast_snapshot (true);
  if (last_heartbeat_ms_ < 0 || now - last_heartbeat_ms_ >= heartbeat_interval_ms)
    {
      QByteArray const heartbeat = QByteArrayLiteral (": heartbeat\n\n");
      QList<QTcpSocket *> sockets;
      for (Client * client : clients_.values ())
        if (client->sse) sockets.append (client->socket);
      for (QTcpSocket * socket : sockets)
        if (Client * client = clients_.value (socket, nullptr)) send_sse (client, heartbeat);
      last_heartbeat_ms_ = now;
    }
}
