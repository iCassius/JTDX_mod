#include "JtdxWebServer.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#include <cstdio>

namespace {
int failures = 0;
int status (QByteArray const& response);

void check (bool condition, char const * message)
{
  if (!condition)
    {
      qCritical () << message;
      std::fprintf (stderr, "FAIL: %s\n", message);
      ++failures;
    }
}

QByteArray request (quint16 port, QByteArray const& target,
                    QByteArray const& token = {}, QByteArray const& extra = {},
                    int timeout_ms = 2000)
{
  QTcpSocket socket;
  QEventLoop loop;
  QByteArray response;
  QTimer timer;
  timer.setSingleShot (true);
  QObject::connect (&socket, &QTcpSocket::readyRead, [&] { response += socket.readAll (); });
  QObject::connect (&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
  QObject::connect (&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  socket.connectToHost (QHostAddress::LocalHost, port);
  if (!socket.waitForConnected (timeout_ms)) return response;
  QByteArray wire = QByteArrayLiteral ("GET ") + target + QByteArrayLiteral (" HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\n");
  if (!token.isEmpty ()) wire += QByteArrayLiteral ("Authorization: Bearer ") + token + QByteArrayLiteral ("\r\n");
  wire += extra + QByteArrayLiteral ("\r\n");
  socket.write (wire);
  socket.flush ();
  timer.start (timeout_ms);
  loop.exec ();
  response += socket.readAll ();
  return response;
}

QByteArray raw_request (quint16 port, QByteArray const& wire, int timeout_ms = 2000)
{
  QTcpSocket socket;
  QEventLoop loop;
  QByteArray response;
  QTimer timer;
  timer.setSingleShot (true);
  QObject::connect (&socket, &QTcpSocket::readyRead, [&] { response += socket.readAll (); });
  QObject::connect (&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
  QObject::connect (&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  socket.connectToHost (QHostAddress::LocalHost, port);
  if (!socket.waitForConnected (timeout_ms)) return response;
  socket.write (wire);
  socket.flush ();
  timer.start (timeout_ms);
  loop.exec ();
  response += socket.readAll ();
  return response;
}

bool partial_header_times_out (quint16 port)
{
  QTcpSocket socket;
  QEventLoop loop;
  QTimer timer;
  QByteArray response;
  timer.setSingleShot (true);
  QObject::connect (&socket, &QTcpSocket::readyRead, [&] { response += socket.readAll (); });
  QObject::connect (&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
  QObject::connect (&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  socket.connectToHost (QHostAddress::LocalHost, port);
  if (!socket.waitForConnected (1000)) return false;
  socket.write (QByteArrayLiteral ("GET / HTTP/1.1\r\n"));
  socket.flush ();
  timer.start (6500);
  loop.exec ();
  response += socket.readAll ();
  return status (response) == 408;
}

QByteArray sse_request (quint16 port, QByteArray const& token, QByteArray const& last_event_id,
                        int timeout_ms = 1000)
{
  QTcpSocket socket;
  QEventLoop loop;
  QByteArray response;
  QTimer timer;
  timer.setSingleShot (true);
  QObject::connect (&socket, &QTcpSocket::readyRead, [&] {
    response += socket.readAll ();
    int const snapshot_event = response.indexOf (QByteArrayLiteral ("event: snapshot\n"));
    int const data = snapshot_event < 0 ? -1 : response.indexOf (QByteArrayLiteral ("data: "), snapshot_event);
    int const end = data < 0 ? -1 : response.indexOf (QByteArrayLiteral ("\n\n"), data);
    if (data >= 0 && end > data)
      {
        QJsonDocument parsed = QJsonDocument::fromJson (response.mid (data + 6, end - data - 6));
        QJsonArray rows = parsed.object ().value (QStringLiteral ("recent_decodes")).toArray ();
        if (parsed.isObject () && parsed.object ().contains (QStringLiteral ("server_epoch"))
            && (!last_event_id.isEmpty () ? response.contains (QByteArrayLiteral ("event: resync_required\n")) : true)
            && rows.size () == 500) loop.quit ();
      }
  });
  QObject::connect (&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  socket.connectToHost (QHostAddress::LocalHost, port);
  if (!socket.waitForConnected (timeout_ms)) return response;
  QByteArray wire = QByteArrayLiteral ("GET /api/v1/events HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ")
      + token + QByteArrayLiteral ("\r\n");
  if (!last_event_id.isEmpty ()) wire += QByteArrayLiteral ("Last-Event-ID: ") + last_event_id + QByteArrayLiteral ("\r\n");
  wire += QByteArrayLiteral ("\r\n");
  socket.write (wire);
  socket.flush ();
  timer.start (timeout_ms);
  loop.exec ();
  response += socket.readAll ();
  socket.disconnectFromHost ();
  return response;
}

int status (QByteArray const& response)
{
  QList<QByteArray> parts = response.left (response.indexOf ('\n')).split (' ');
  return parts.size () >= 2 ? parts.at (1).toInt () : 0;
}
}

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  JtdxWebState state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("test-instance")};
  state.observe_status (14074000, QStringLiteral ("FT8"), QStringLiteral ("K1ABC"), QStringLiteral ("-10"),
                        QStringLiteral ("FT8"), true, false, true, -100, 150, QStringLiteral ("N0CALL"),
                        QStringLiteral ("AA00"), QStringLiteral ("FN31"), false, {}, false, false);
  JtdxWebServer server {&state};
  JtdxWebServer::Configuration config;
  check (!server.is_listening (), "server must be lazy and stopped by default");
  check (server.start (config), "automatic loopback server should start");
  check (server.is_listening () && server.actual_port () >= JtdxWebServer::automatic_port_first
             && server.actual_port () <= JtdxWebServer::automatic_port_last,
         "automatic port must be bounded");
  quint16 const port = server.actual_port ();
  QByteArray const token = server.bearer_token_for_testing ().toUtf8 ();
  check (token.size () >= 32, "default token must have high entropy length");

  QByteArray response = request (port, QByteArrayLiteral ("/"));
  check (status (response) == 200 && response.contains (QByteArrayLiteral ("Read-only service")), "root must be safe guidance");
  check (status (request (port, QByteArrayLiteral ("/healthz"))) == 401, "health must require bearer token");
  response = request (port, QByteArrayLiteral ("/healthz"), token);
  check (status (response) == 200 && !response.contains (QByteArrayLiteral ("online")), "health must describe service only");
  response = request (port, QByteArrayLiteral ("/api/v1/state"), token);
  check (status (response) == 200, "state endpoint must respond");
  QJsonDocument state_document = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  check (state_document.isObject () && state_document.object ().value (QStringLiteral ("server_epoch")).toString () == server.server_epoch(),
         "state must project server epoch");
  check (state_document.object ().value (QStringLiteral ("web_server_state")).toString () == QStringLiteral ("listening"),
         "state must project web server state without changing business freshness");
  check (status (request (port, QByteArrayLiteral ("/api/v1/control/frequency"), token,
                         QByteArrayLiteral ("Content-Length: 0\r\n"))) == 400,
         "body framing must be rejected before any control path");
  quint64 const revision_before_post = state.revision ();
  QByteArray const post = QByteArrayLiteral ("POST /api/v1/control/frequency HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token + QByteArrayLiteral ("\r\n\r\n");
  int const post_status = status (raw_request (port, post));
  check (post_status == 404 || post_status == 405,
         "POST/control routes must never execute");
  check (state.revision () == revision_before_post, "rejected control routes must not mutate state");
  check (status (request (port, QByteArrayLiteral ("/api/v1/state?token=leak"), token)) == 400,
         "query token must be rejected");
  check (status (request (port, QByteArrayLiteral ("/api/v1/state"), token,
                         QByteArrayLiteral ("Origin: https://evil.invalid\r\n"))) == 403,
         "cross origin must be rejected");
  QByteArray const evil_host = QByteArrayLiteral ("GET /api/v1/state HTTP/1.1\r\nHost: evil.invalid:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token + QByteArrayLiteral ("\r\n\r\n");
  check (status (raw_request (port, evil_host)) == 400,
         "unexpected host must be rejected");

  state.set_decode_limit (500);
  for (int i = 0; i < 500; ++i)
    state.observe_decode (true, QTime {12, 34, 0}, -10, 0.1F, static_cast<quint32> (i),
                          QStringLiteral ("FT8"), QStringLiteral ("CQ N0CALL FN31 ") + QString (220, QChar {'X'}),
                          false, false, QStringLiteral ("N0CALL"), QStringLiteral ("FN31"));
  response = request (port, QByteArrayLiteral ("/api/v1/decodes"), token, {}, 5000);
  QJsonDocument decode_document = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  QJsonArray decodes = decode_document.object ().value (QStringLiteral ("decodes")).toArray ();
  check (status (response) == 200 && decodes.size () == 500, "full 500 decode snapshot must succeed over HTTP");
  int const full_snapshot_bytes = QJsonDocument {state.json_snapshot ()}.toJson (QJsonDocument::Compact).size ();
  check (full_snapshot_bytes < JtdxWebServer::max_sse_event_bytes,
         "measured full 500 decode snapshot must fit the explicit SSE event bound");

  response = sse_request (port, token, {});
  check (response.startsWith (QByteArrayLiteral ("HTTP/1.1 200"))
             && response.contains (QByteArrayLiteral ("event: snapshot\n")),
         "SSE initial snapshot must succeed");
  response = sse_request (port, token, QByteArrayLiteral ("old-epoch-1"));
  check (response.contains (QByteArrayLiteral ("event: resync_required\n"))
             && response.contains (QByteArrayLiteral ("event: snapshot\n")),
         "SSE Last-Event-ID must request bounded resync plus full snapshot");
  check (partial_header_times_out (port), "partial header must be bounded by the 5 second deadline");

  QList<QTcpSocket *> held;
  for (int i = 0; i < JtdxWebServer::max_connections + 2; ++i)
    {
      auto * socket = new QTcpSocket;
      socket->connectToHost (QHostAddress::LocalHost, port);
      check (socket->waitForConnected (1000), "connection-limit fixture client must connect");
      held.append (socket);
    }
  QByteArray const limited = raw_request (port,
      QByteArrayLiteral ("GET / HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\n\r\n"));
  check (status (limited) == 503, "the seventeenth client must be rejected at the bounded connection limit");
  for (QTcpSocket * socket : held)
    {
      socket->abort ();
      delete socket;
    }
  check (status (request (port, QByteArrayLiteral ("/healthz"), token)) == 200,
         "connection capacity must recover after clients close");

  int const child_count = server.children ().size ();
  for (int i = 0; i < 20; ++i)
    {
      request (port, QByteArrayLiteral ("/healthz"), token);
      QCoreApplication::processEvents (QEventLoop::AllEvents, 50);
    }
  check (server.children ().size () <= child_count + 1, "completed requests must not leak timer/socket children");

  // A client that never reads is disconnected when the bounded SSE queue fills.
  QTcpSocket slow;
  slow.setReadBufferSize (1);
  QEventLoop slow_loop;
  QTimer slow_timer;
  slow_timer.setSingleShot (true);
  QObject::connect (&slow, &QTcpSocket::disconnected, &slow_loop, &QEventLoop::quit);
  QObject::connect (&slow_timer, &QTimer::timeout, &slow_loop, &QEventLoop::quit);
  slow.connectToHost (QHostAddress::LocalHost, port);
  check (slow.waitForConnected (1000), "slow SSE client must connect");
  slow.write (QByteArrayLiteral ("GET /api/v1/events HTTP/1.1\r\nHost: 127.0.0.1:")
              + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token + QByteArrayLiteral ("\r\n\r\n"));
  slow.flush ();
  slow_timer.start (25000);
  slow_loop.exec ();
  if (server.active_connection_count () != 0)
    std::fprintf (stderr, "slow active count=%d socket_state=%d\\n", server.active_connection_count (), static_cast<int> (slow.state ()));
  check (server.active_connection_count () == 0, "slow SSE client must be bounded by backpressure and released server-side");
  check (status (request (port, QByteArrayLiteral ("/healthz"), token)) == 200,
         "server must remain responsive after slow SSE eviction");
  slow.abort ();

  QString const old_epoch = server.server_epoch ();
  server.stop ();
  check (!server.is_listening () && server.web_server_state () == QStringLiteral ("stopped"), "stop must close listener");
  check (server.start (config) && server.server_epoch () != old_epoch, "restart must create a new epoch");
  server.stop ();

  QTcpServer manual_probe;
  check (manual_probe.listen (QHostAddress::LocalHost, 0), "manual port probe should bind");
  quint16 const manual_port = manual_probe.serverPort ();
  manual_probe.close ();
  JtdxWebServer manual_server {&state};
  JtdxWebServer::Configuration manual_ok;
  manual_ok.automatic_port = false;
  manual_ok.port = manual_port;
  check (manual_server.start (manual_ok) && manual_server.actual_port () == manual_port,
         "manual TCP port should start on the requested port");
  manual_server.stop ();

  QTcpServer auto_occupied;
  if (auto_occupied.listen (QHostAddress::LocalHost, JtdxWebServer::automatic_port_first))
    {
      JtdxWebServer auto_server {&state};
      JtdxWebServer::Configuration auto_skip;
      auto_skip.udp_ports.insert (JtdxWebServer::automatic_port_first + 1);
      check (auto_server.start (auto_skip), "automatic server should find a bounded free port");
      check (auto_server.actual_port () != JtdxWebServer::automatic_port_first
                 && auto_server.actual_port () != JtdxWebServer::automatic_port_first + 1,
             "automatic port search must skip occupied and numeric UDP ports");
      auto_server.stop ();
      auto_occupied.close ();
    }

  auto * disappearing_state = new JtdxWebState {QStringLiteral ("JTDX"), QStringLiteral ("test")};
  JtdxWebServer disappearing_server {disappearing_state};
  check (disappearing_server.start (config), "lifetime server fixture should start");
  delete disappearing_state;
  QCoreApplication::processEvents (QEventLoop::AllEvents, 100);
  check (!disappearing_server.is_listening () && disappearing_server.active_connection_count () == 0,
         "destroyed state must stop the server before callbacks can use it");

  QTcpServer occupied;
  check (occupied.listen (QHostAddress::LocalHost, 0), "test occupied TCP port should bind");
  JtdxWebServer conflict {&state};
  JtdxWebServer::Configuration manual;
  manual.automatic_port = false;
  manual.port = occupied.serverPort ();
  check (!conflict.start (manual) && conflict.web_server_state () == QStringLiteral ("error"), "occupied manual port must fail");
  manual.port = 49152;
  manual.udp_ports.insert (manual.port);
  check (!conflict.start (manual), "numeric UDP port exclusion must not bind UDP and must reject manual collision");
  occupied.close ();

  if (failures)
    qCritical () << failures << "Web server checks failed";
  else
    qDebug () << "Web server checks passed; measured full snapshot bytes:" << full_snapshot_bytes;
  return failures ? 1 : 0;
}
