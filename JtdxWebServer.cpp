#include "JtdxWebServer.hpp"

#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int header_timeout_ms = 5000;
constexpr int publish_interval_ms = 1000;
constexpr qint64 snapshot_interval_ms = 2000;
constexpr qint64 heartbeat_interval_ms = 10000;
constexpr int response_drain_timeout_ms = 2000;

bool valid_digest (QString const& digest)
{
  if (digest.size () != 64) return false;
  for (QChar const ch : digest)
    if (!ch.isDigit () && !(ch >= QChar {'a'} && ch <= QChar {'f'})
        && !(ch >= QChar {'A'} && ch <= QChar {'F'})) return false;
  return true;
}

QByteArray status_reason (int status)
{
  switch (status)
    {
    case 200: return QByteArrayLiteral ("OK");
    case 400: return QByteArrayLiteral ("Bad Request");
    case 401: return QByteArrayLiteral ("Unauthorized");
    case 403: return QByteArrayLiteral ("Forbidden");
    case 404: return QByteArrayLiteral ("Not Found");
    case 405: return QByteArrayLiteral ("Method Not Allowed");
    case 408: return QByteArrayLiteral ("Request Timeout");
    case 413: return QByteArrayLiteral ("Payload Too Large");
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
  connect (&server_, &QTcpServer::newConnection, this, &JtdxWebServer::accept_connections);
  connect (publish_timer_, &QTimer::timeout, this, &JtdxWebServer::on_publish_timer);
  if (state_)
    {
      connect (state_, &QObject::destroyed, this, [this] { stop (); });
    }
}

JtdxWebServer::~JtdxWebServer ()
{
  stop ();
}

bool JtdxWebServer::validate_configuration (Configuration const& configuration,
                                             QString * error) const
{
  bool const loopback = configuration.bind_address.isLoopback ();
  if (!loopback && !configuration.allow_lan)
    {
      if (error) *error = QStringLiteral ("LAN binding requires allow_lan");
      return false;
    }
  if (configuration.allow_lan
      && (configuration.bind_address == QHostAddress::Any
          || configuration.bind_address == QHostAddress::AnyIPv4
          || configuration.bind_address == QHostAddress::AnyIPv6))
    {
      if (error) *error = QStringLiteral ("P2 LAN binding requires an explicit local address");
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
  QString digest = configuration.bearer_token_sha256.trimmed ();
  if (digest.isEmpty () && !configuration.bearer_token.isEmpty ())
    digest = bearer_token_digest (configuration.bearer_token);
  if (!configuration.bearer_token.isEmpty () && configuration.allow_lan
      && configuration.bearer_token.toUtf8 ().size () < 32)
    {
      if (error) *error = QStringLiteral ("LAN bearer token is too short");
      return false;
    }
  QByteArray const digest_bytes = QByteArray::fromHex (digest.toLatin1 ());
  if (!valid_digest (digest) || digest_bytes.size () != 32)
    {
      if (error) *error = QStringLiteral ("a valid SHA-256 bearer token digest is required");
      return false;
    }
  if (configuration.allow_lan && configuration.allowed_origin.isEmpty())
    {
      if (error) *error = QStringLiteral ("LAN binding requires an exact allowed_origin");
      return false;
    }
  if (configuration.allow_lan)
    {
      QUrl const origin {configuration.allowed_origin};
      QString const expected_host = configuration.bind_address.toString ();
      if (!origin.isValid () || origin.scheme () != QStringLiteral ("http")
          || origin.host ().compare (expected_host, Qt::CaseInsensitive) != 0
          || !origin.userInfo ().isEmpty () || !origin.query ().isEmpty ()
          || (origin.path () != QString {} && origin.path () != QStringLiteral ("/"))
          || origin.fragment () != QString {})
        {
          if (error) *error = QStringLiteral ("allowed_origin must be http and match the bind address");
          return false;
        }
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
  if (configuration.bearer_token_sha256.isEmpty () && !configuration.bearer_token.isEmpty ())
    configuration.bearer_token_sha256 = bearer_token_digest (configuration.bearer_token);
  configuration.bearer_token.clear ();
  bearer_token_digest_ = QByteArray::fromHex (configuration.bearer_token_sha256.trimmed ().toLatin1 ());
  if (bearer_token_digest_.size () != 32)
    {
      last_error_ = QStringLiteral ("a valid SHA-256 bearer token digest is required");
      web_server_state_ = QStringLiteral ("error");
      return false;
    }
  configuration_ = configuration;
  server_epoch_ = new_epoch ();
  if (choose_and_listen (configuration)) return true;
  last_error_ = server_.errorString ();
  if (last_error_.isEmpty ()) last_error_ = QStringLiteral ("unable to bind TCP port");
  web_server_state_ = QStringLiteral ("error");
  server_.close ();
  actual_address_ = QHostAddress {};
  actual_port_ = 0;
  return false;
}

void JtdxWebServer::stop ()
{
  if (publish_timer_) publish_timer_->stop ();
  server_.close ();
  QList<QTcpSocket *> sockets = clients_.keys ();
  for (QTcpSocket * socket : sockets) close_client (socket);
  actual_address_ = QHostAddress {};
  actual_port_ = 0;
  web_server_state_ = QStringLiteral ("stopped");
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

QString JtdxWebServer::bearer_token_digest (QString const& bearer_token)
{
  return QString::fromLatin1 (QCryptographicHash::hash (bearer_token.toUtf8 (), QCryptographicHash::Sha256).toHex ());
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
  if (client->request.size () > max_header_bytes)
    {
      reject_connection (socket, 431, QByteArrayLiteral ("header limit"));
      return;
    }
  int const header_end = client->request.indexOf (QByteArrayLiteral ("\r\n\r\n"));
  if (header_end < 0) return;
  if (header_end + 4 != client->request.size ())
    {
      reject_connection (socket, 400, QByteArrayLiteral ("request body or pipelining is not supported"));
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
  process_request (socket, request);
}

bool JtdxWebServer::host_allowed (QByteArray const& host) const
{
  QString value = QString::fromUtf8 (host).trimmed ();
  if (value.isEmpty () || value.contains (QRegExp (QStringLiteral ("[\\s/@,]")))) return false;
  QString hostname;
  QString port;
  if (value.startsWith (QChar {'['}))
    {
      int const close = value.indexOf (QChar {']'});
      if (close < 0 || close + 1 >= value.size () || value.at (close + 1) != QChar {':'}) return false;
      hostname = value.mid (1, close - 1);
      port = value.mid (close + 2);
    }
  else
    {
      int const colon = value.lastIndexOf (QChar {':'});
      if (colon <= 0) return false;
      hostname = value.left (colon);
      port = value.mid (colon + 1);
      if (hostname.contains (QChar {':'})) return false;
    }
  bool ok = false;
  quint16 const parsed_port = port.toUShort (&ok);
  if (!ok || parsed_port != actual_port_ || hostname.isEmpty ()) return false;
  if (!configuration_.allow_lan)
    {
      return hostname.compare (QStringLiteral ("localhost"), Qt::CaseInsensitive) == 0
          || hostname == QStringLiteral ("127.0.0.1")
          || hostname == QStringLiteral ("::1");
    }
  if (hostname == QStringLiteral ("0.0.0.0") || hostname == QStringLiteral ("::")) return false;
  bool const any_address = configuration_.bind_address == QHostAddress::Any
      || configuration_.bind_address == QHostAddress::AnyIPv4
      || configuration_.bind_address == QHostAddress::AnyIPv6;
  QString expected = configuration_.bind_address.toString ();
  if (!any_address)
    return hostname.compare (expected, Qt::CaseInsensitive) == 0;
  QUrl const origin {configuration_.allowed_origin};
  return origin.isValid () && hostname.compare (origin.host (), Qt::CaseInsensitive) == 0;
}

bool JtdxWebServer::origin_allowed (QByteArray const& origin) const
{
  if (origin.isEmpty ()) return true;
  QString const value = QString::fromUtf8 (origin);
  if (configuration_.allow_lan)
    {
      QUrl const origin {configuration_.allowed_origin};
      QString expected = origin.scheme () + QStringLiteral ("://");
      QString host = configuration_.bind_address.toString ();
      if (configuration_.bind_address.protocol () == QAbstractSocket::IPv6Protocol)
        host = QStringLiteral ("[") + host + QStringLiteral ("]");
      expected += host + QStringLiteral (":") + QString::number (actual_port_);
      return value == expected;
    }
  QString const base = url ();
  QString const local = QStringLiteral ("http://localhost:") + QString::number (actual_port_);
  QString const ipv4 = QStringLiteral ("http://127.0.0.1:") + QString::number (actual_port_);
  QString const ipv6 = QStringLiteral ("http://[::1]:") + QString::number (actual_port_);
  return value == base || value == local || value == ipv4 || value == ipv6;
}

bool JtdxWebServer::authorized (QHash<QByteArray, QByteArray> const& headers) const
{
  QByteArray const value = headers.value (QByteArrayLiteral ("authorization"));
  QByteArray const prefix = QByteArrayLiteral ("Bearer ");
  if (!value.startsWith (prefix)) return false;
  QByteArray const supplied = QCryptographicHash::hash (value.mid (prefix.size ()), QCryptographicHash::Sha256);
  if (supplied.size () != bearer_token_digest_.size ()) return false;
  unsigned char diff {0};
  for (int i = 0; i < supplied.size (); ++i)
    diff = static_cast<unsigned char> (diff | static_cast<unsigned char> (supplied.at (i) ^ bearer_token_digest_.at (i)));
  return diff == 0;
}

QByteArray JtdxWebServer::event_id () const
{
  return server_epoch_.toUtf8 () + QByteArrayLiteral ("-")
       + QByteArray::number (state_ ? state_->revision () : 0);
}

QJsonObject JtdxWebServer::state_snapshot () const
{
  QJsonObject snapshot = state_ ? state_->json_snapshot () : QJsonObject {};
  snapshot.insert (QStringLiteral ("server_epoch"), server_epoch_);
  snapshot.insert (QStringLiteral ("web_server_state"), web_server_state_);
  return snapshot;
}

QByteArray JtdxWebServer::json_response (QJsonObject const& object) const
{
  return QJsonDocument {object}.toJson (QJsonDocument::Compact);
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
  if (payload.size () > max_sse_event_bytes
      || client->pending.size () + payload.size () > max_sse_pending_bytes
      || client->socket->bytesToWrite () + client->pending.size () + payload.size () > max_sse_pending_bytes)
    {
      close_client (client->socket);
      return;
    }
  client->pending += payload;
  pump_sse (client);
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
  QList<QByteArray> lines = request.left (request.size () - 4).split ('\n');
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
  if (!headers.contains (QByteArrayLiteral ("host")) || !host_allowed (headers.value (QByteArrayLiteral ("host"))))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("invalid host"));
      return;
    }
  if (!origin_allowed (headers.value (QByteArrayLiteral ("origin"))))
    {
      reject_connection (socket, 403, QByteArrayLiteral ("origin denied"));
      return;
    }
  if (headers.contains (QByteArrayLiteral ("content-length"))
      || headers.contains (QByteArrayLiteral ("transfer-encoding"))
      || headers.contains (QByteArrayLiteral ("expect"))
      || headers.contains (QByteArrayLiteral ("upgrade")))
    {
      reject_connection (socket, 400, QByteArrayLiteral ("request body or upgrade is unsupported"));
      return;
    }
  if (method != QByteArrayLiteral ("GET"))
    {
      reject_connection (socket, 405, QByteArrayLiteral ("GET only"));
      return;
    }
  QByteArray const path = target;
  if (path == QByteArrayLiteral ("/"))
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
  if (path == QByteArrayLiteral ("/style.css") || path == QByteArrayLiteral ("/app.js"))
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
  if (!authorized (headers))
    {
      reject_connection (socket, 401, QByteArrayLiteral ("bearer authentication required"));
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
  last_snapshot_ms_ = now;
}

void JtdxWebServer::on_publish_timer ()
{
  if (!is_listening ()) return;
  qint64 const now = activity_clock_.elapsed ();
  quint64 const revision = state_ ? state_->revision () : 0;
  if (last_snapshot_ms_ < 0 || revision != last_published_revision_
      || now - last_snapshot_ms_ >= snapshot_interval_ms)
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
