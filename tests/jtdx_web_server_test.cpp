#include "JtdxWebServer.hpp"
#include "Bands.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
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

struct ParsedSseEvent
{
  QByteArray name;
  QByteArray id;
  QJsonObject data;
};

QVector<ParsedSseEvent> parse_sse_events (QByteArray const& response)
{
  QVector<ParsedSseEvent> events;
  int offset = 0;
  while (offset < response.size ())
    {
      int const end = response.indexOf (QByteArrayLiteral ("\n\n"), offset);
      if (end < 0) break;
      QByteArray const block = response.mid (offset, end - offset);
      int const event_pos = block.indexOf (QByteArrayLiteral ("event: "));
      int const id_pos = block.indexOf (QByteArrayLiteral ("id: "));
      int const data_pos = block.indexOf (QByteArrayLiteral ("data: "));
      int const data_end = block.indexOf ('\n', data_pos) < 0 ? block.size () : block.indexOf ('\n', data_pos);
      if (event_pos >= 0 && id_pos >= 0 && data_pos >= 0 && data_end > data_pos)
        {
          QJsonDocument document = QJsonDocument::fromJson (block.mid (data_pos + 6, data_end - data_pos - 6));
          if (document.isObject ())
            {
              ParsedSseEvent event;
              event.name = block.mid (event_pos + 7, block.indexOf ('\n', event_pos) - event_pos - 7);
              event.id = block.mid (id_pos + 4, block.indexOf ('\n', id_pos) - id_pos - 4);
              event.data = document.object ();
              events.append (std::move (event));
            }
        }
      offset = end + 2;
    }
  return events;
}

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

QByteArray post_frequency (quint16 port, QByteArray const& token, QByteArray const& body,
                           QByteArray const& origin, bool fragmented = false)
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
  if (!socket.waitForConnected (1000)) return response;
  QByteArray wire = QByteArrayLiteral ("POST /api/v1/control/frequency HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token
      + QByteArrayLiteral ("\r\nOrigin: ") + origin
      + QByteArrayLiteral ("\r\nContent-Type: application/json\r\nContent-Length: ")
      + QByteArray::number (body.size ()) + QByteArrayLiteral ("\r\n\r\n");
  socket.write (wire);
  if (fragmented)
    {
      socket.flush ();
      QCoreApplication::processEvents (QEventLoop::AllEvents, 10);
      for (int i = 0; i < body.size (); i += 3)
        {
          socket.write (body.mid (i, 3));
          socket.flush ();
          QCoreApplication::processEvents (QEventLoop::AllEvents, 5);
        }
    }
  else socket.write (body);
  socket.flush ();
  timer.start (2000);
  loop.exec ();
  response += socket.readAll ();
  return response;
}

QByteArray post_select_dx (quint16 port, QByteArray const& token, QByteArray const& body,
                           QByteArray const& origin)
{
  QByteArray wire = QByteArrayLiteral ("POST /api/v1/control/select-dx HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token
      + QByteArrayLiteral ("\r\nOrigin: ") + origin
      + QByteArrayLiteral ("\r\nContent-Type: application/json\r\nContent-Length: ")
      + QByteArray::number (body.size ()) + QByteArrayLiteral ("\r\n\r\n") + body;
  return raw_request (port, wire);
}

QByteArray post_business (quint16 port, QByteArray const& token, QByteArray const& path,
                          QByteArray const& body, QByteArray const& origin)
{
  QByteArray wire = QByteArrayLiteral ("POST /api/v1/control/") + path
      + QByteArrayLiteral (" HTTP/1.1\r\nHost: 127.0.0.1:") + QByteArray::number (port)
      + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token + QByteArrayLiteral ("\r\nOrigin: ")
      + origin + QByteArrayLiteral ("\r\nContent-Type: application/json\r\nContent-Length: ")
      + QByteArray::number (body.size ()) + QByteArrayLiteral ("\r\n\r\n") + body;
  return raw_request (port, wire);
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
  state.observe_decode (true, QTime {12, 34, 56}, -10, 0.1F, 1500,
                        QStringLiteral ("FT8"), QStringLiteral ("K1ABC FN31"), false, false,
                        QStringLiteral ("K1ABC"), QStringLiteral ("FN31"));
  JtdxWebServer server {&state};
  JtdxWebServer::Configuration config;
  QByteArray const token = QByteArrayLiteral ("p3-test-token-0123456789-abcdefghijklmnopqrstuvwxyz");
  config.bearer_token_sha256 = QString::fromLatin1 (QCryptographicHash::hash (token, QCryptographicHash::Sha256).toHex ());
  JtdxWebControl control {1000};
  control.set_clock_for_test (0);
  JtdxWebControl::ObservedState observed;
  observed.safety.known = true;
  observed.safety.fresh = true;
  observed.safety.rig_online = true;
  observed.safety.monitoring = true;
  observed.safety.business_state_known = true;
  observed.state_revision = state.revision ();
  observed.frequency_generation = 1;
  observed.frequency_known = true;
  observed.actual_frequency_hz = 14074000;
  control.set_observed_state (observed);
  JtdxWebControl::Dispatch captured_dispatch;
  JtdxWebControl::Dispatch captured_dx_dispatch;
  control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      captured_dispatch = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "TCP frequency request must reach production prepare dispatcher");
      check (control.begin_dispatch (prepared),
             "TCP frequency request must reach production begin dispatcher");
    });
  control.set_select_dx_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      captured_dx_dispatch = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "TCP DX request must reach production prepare dispatcher");
      check (control.begin_dispatch (prepared),
             "TCP DX request must reach production begin dispatcher");
    });
  server.set_control (&control);
  Bands bands;
  server.set_frequency_validator ([&] (QString const& input) {
      return JtdxWebFrequency::parse_and_validate_hz (input, bands);
    });
  config.enable_frequency_control = true;
  config.enable_dx_control = true;
  config.enable_automation_control = true;
  bool const browser_automation_fixture = app.arguments ().contains (
      QStringLiteral ("--serve-browser-automation"));
  if (browser_automation_fixture)
    {
      // Loopback-only browser fixture.  It drives the production HTTP,
      // Control and State objects with an in-memory business adapter; no
      // MainWindow, CAT, PTT, UDP or audio path is created.
      state.set_clock_for_test (0);
      state.observe_status (14074000, QStringLiteral ("FT8"), {}, QStringLiteral ("-10"),
                            QStringLiteral ("FT8"), false, false, true, -100, 150,
                            QStringLiteral ("N0CALL"), QStringLiteral ("AA00"), {}, false, {}, false, false);
      state.observe_rig (true, 14074000, 14074000, false);
      state.observe_business_state (false, QStringLiteral ("idle"), QStringLiteral ("idle"), {});
      state.set_frequency_candidates (
          QStringLiteral ("FT8"), QStringLiteral ("All"),
          {{7074000u, QStringLiteral ("40m"), QStringLiteral ("FT8"), QStringLiteral ("All"), true},
           {14074000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("All"), true}});
      control.set_observation_provider ([&state] {
          QJsonObject const snapshot = state.json_snapshot ();
          JtdxWebControl::ObservedState current;
          current.safety.known = snapshot.value (QStringLiteral ("freshness")).toString () == QStringLiteral ("fresh");
          current.safety.fresh = current.safety.known;
          current.safety.rig_online = snapshot.value (QStringLiteral ("rig_online")).toBool ();
          current.safety.monitoring = true;
          current.safety.tx_enabled = snapshot.value (QStringLiteral ("tx_enabled")).toBool ();
          current.safety.transmitting = snapshot.value (QStringLiteral ("transmitting")).toBool ();
          current.safety.ptt = snapshot.value (QStringLiteral ("ptt")).toBool ();
          current.safety.watchdog_timeout = snapshot.value (QStringLiteral ("watchdog_timeout")).toBool ();
          current.safety.business_state_known = snapshot.value (QStringLiteral ("auto_sequence_state")).isString ();
          current.state_revision = state.revision ();
          current.business_generation = snapshot.value (QStringLiteral ("business_generation")).toVariant ().toULongLong ();
          current.business_state_known = current.safety.business_state_known;
          current.auto_sequence_enabled = snapshot.value (QStringLiteral ("auto_sequence_state")).toString () == QStringLiteral ("enabled");
          current.cq_state = snapshot.value (QStringLiteral ("cq_state")).toString ();
          current.frequency_generation = state.rig_generation ();
          current.frequency_known = snapshot.value (QStringLiteral ("frequency")).isDouble ();
          current.actual_frequency_hz = snapshot.value (QStringLiteral ("frequency")).toVariant ().toLongLong ();
          return current;
        });
      control.set_business_dispatcher ([&state, &control] (JtdxWebControl::Dispatch const& dispatch) {
          JtdxWebControl::Dispatch prepared;
          if (!control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared)
              || !control.begin_dispatch (prepared)) return;
          QTimer::singleShot (250, &state, [&state, &control, prepared] {
              if (prepared.operation == JtdxWebControl::Operation::StartCq)
                state.observe_business_state (false, QStringLiteral ("calling"), QStringLiteral ("armed"),
                                              QStringLiteral ("CQ N0CALL FN31"));
              else if (prepared.operation == JtdxWebControl::Operation::StartAutoCall)
                state.observe_business_state (true, QStringLiteral ("calling"), QStringLiteral ("armed"),
                                              QStringLiteral ("CQ N0CALL FN31"));
              else
                state.observe_business_state (false, QStringLiteral ("idle"), QStringLiteral ("idle"), {});
              auto const snapshot = state.json_snapshot ();
              control.feedback_business (prepared.request_id, prepared.server_epoch,
                                         snapshot.value (QStringLiteral ("business_generation")).toVariant ().toULongLong (),
                                         snapshot.value (QStringLiteral ("cq_state")).toString (),
                                         snapshot.value (QStringLiteral ("auto_sequence_state")).toString () == QStringLiteral ("enabled"),
                                         state.revision ());
            });
        });
      config.automatic_port = true;
      if (!server.start (config)) return 2;
      control.bind_server_epoch (server.server_epoch ());
      QByteArray const fixture_url = server.url ().toUtf8 () + QByteArrayLiteral ("/#fixture");
      std::fprintf (stdout, "WEB_UI_FIXTURE_URL=%s\nWEB_UI_FIXTURE_TOKEN=%s\n",
                    fixture_url.constData (), token.constData ());
      std::fflush (stdout);
      QTimer::singleShot (300000, &app, &QCoreApplication::quit);
      return app.exec ();
    }
  bool const browser_frequency_fixture = app.arguments ().contains (
      QStringLiteral ("--serve-browser-frequency"))
      || app.arguments ().contains (QStringLiteral ("--serve-browser-frequency-empty"))
      || app.arguments ().contains (QStringLiteral ("--serve-browser-frequency-invalidated"));
  if (browser_frequency_fixture)
    {
      // This branch is a loopback-only browser fixture.  It uses the production
      // State/Control/Server path while keeping all radio I/O out of the test.
      state.set_clock_for_test (0);
      state.observe_status (14074000, QStringLiteral ("FT8"), QStringLiteral ("K1ABC"), QStringLiteral ("-10"),
                            QStringLiteral ("FT8"), false, false, true, -100, 150, QStringLiteral ("N0CALL"),
                            QStringLiteral ("AA00"), QStringLiteral ("FN31"), false, {}, false, false);
      state.observe_rig (true, 14074000, 14074000, false);
      state.observe_business_state (false, QStringLiteral ("idle"), QStringLiteral ("idle"), {});
      bool const empty_frequency_fixture = app.arguments ().contains (
          QStringLiteral ("--serve-browser-frequency-empty"));
      bool const invalidated_frequency_fixture = app.arguments ().contains (
          QStringLiteral ("--serve-browser-frequency-invalidated"));
      if (empty_frequency_fixture)
        {
          state.set_frequency_candidates (QStringLiteral ("FT4"), QStringLiteral ("Region 3"), {});
        }
      else
        {
          state.set_frequency_candidates (
              QStringLiteral ("FT8"), QStringLiteral ("All"),
              {{7074000u, QStringLiteral ("40m"), QStringLiteral ("FT8"), QStringLiteral ("All"), true},
               {14074000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("All"), true},
               {14075000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("All"), false},
               {14076000u, QStringLiteral ("20m"), QStringLiteral ("JT65"), QStringLiteral ("All"), false}});
        }

      control.set_observation_provider ([&state] {
          QJsonObject const snapshot = state.json_snapshot ();
          JtdxWebControl::ObservedState observed_from_state;
          QString const freshness = snapshot.value (QStringLiteral ("freshness")).toString ();
          observed_from_state.safety.known = freshness == QStringLiteral ("fresh");
          observed_from_state.safety.fresh = observed_from_state.safety.known;
          observed_from_state.safety.rig_online = snapshot.value (QStringLiteral ("rig_online")).toBool ();
          observed_from_state.safety.monitoring = true;
          observed_from_state.safety.business_state_known = true;
          observed_from_state.safety.tx_enabled = snapshot.value (QStringLiteral ("tx_enabled")).toBool ();
          observed_from_state.safety.transmitting = snapshot.value (QStringLiteral ("transmitting")).toBool ();
          observed_from_state.safety.ptt = snapshot.value (QStringLiteral ("ptt")).toBool ();
          observed_from_state.safety.watchdog_timeout = snapshot.value (QStringLiteral ("watchdog_timeout")).toBool ();
          observed_from_state.state_revision = state.revision ();
          observed_from_state.frequency_generation = state.rig_generation ();
          QJsonValue const frequency = snapshot.value (QStringLiteral ("frequency"));
          qint64 const frequency_hz = frequency.isDouble () ? static_cast<qint64> (frequency.toDouble ()) : 0;
          observed_from_state.frequency_known = frequency_hz > 0;
          observed_from_state.actual_frequency_hz = observed_from_state.frequency_known
              ? frequency_hz : 0;
          return observed_from_state;
        });
      control.set_frequency_dispatcher ([&state, &control] (JtdxWebControl::Dispatch const& dispatch) {
          JtdxWebControl::Dispatch prepared;
          if (!control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared)
              || !control.begin_dispatch (prepared)) return;
          QTimer::singleShot (300, &state, [&state, &control, prepared] {
              state.observe_rig (true, prepared.frequency_hz, prepared.frequency_hz, false);
              control.feedback_frequency (prepared.request_id, prepared.server_epoch,
                                          state.rig_generation (), prepared.frequency_hz,
                                          state.revision ());
            });
        });
      config.automatic_port = true;
      if (!server.start (config)) return 2;
      control.bind_server_epoch (server.server_epoch ());
      QByteArray const fixture_url = server.url ().toUtf8 () + QByteArrayLiteral ("/#fixture");
      std::fprintf (stdout, "WEB_UI_FIXTURE_URL=%s\nWEB_UI_FIXTURE_TOKEN=%s\n",
                    fixture_url.constData (), token.constData ());
      std::fflush (stdout);
      if (invalidated_frequency_fixture)
        {
          QTimer::singleShot (60000, &state, [&state] {
            state.set_frequency_candidates (
                QStringLiteral ("FT8"), QStringLiteral ("Region 2"),
                {{14074000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("Region 2"), true},
                 {14076000u, QStringLiteral ("20m"), QStringLiteral ("FT8"), QStringLiteral ("Region 2"), false}});
          });
        }
      QTimer::singleShot (300000, &app, &QCoreApplication::quit);
      return app.exec ();
    }
  if (app.arguments ().contains (QStringLiteral ("--serve-browser")))
    {
      state.set_clock_for_test (0);
      state.observe_business_state (true, QStringLiteral ("calling"), QStringLiteral ("armed"), QStringLiteral ("CQ N0CALL FN31"));
      for (int i = 0; i < 6; ++i)
        state.observe_decode (true, QTime {12, 34, i}, -10 + i, 0.1F, static_cast<quint32> (100 + i),
                              QStringLiteral ("FT8"), QStringLiteral ("K1ABC FN31 <script>"), false, false,
                              QStringLiteral ("K1ABC"), QStringLiteral ("FN31"));
      observed.state_revision = state.revision ();
      control.set_observed_state (observed);
      control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
          if (dispatch.request_id == QStringLiteral ("fixture-completed"))
            {
              JtdxWebControl::Dispatch prepared;
              if (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared)
                  && control.begin_dispatch (prepared))
                control.feedback_frequency (prepared.request_id, prepared.server_epoch,
                                           prepared.expected_generation + 1, prepared.frequency_hz,
                                           observed.state_revision + 1);
            }
          else if (dispatch.request_id == QStringLiteral ("fixture-failed"))
            {
              control.fail (dispatch.request_id, dispatch.server_epoch, QStringLiteral ("fixture_failed"));
            }
          // fixture-timeout intentionally remains queued without begin_dispatch so it
          // expires without setting the unconfirmed feedback latch. fixture-pending is
          // submitted last and remains pending for the browser operations view.
        });
      config.automatic_port = true;
      config.enable_frequency_control = false;
      if (!server.start (config)) return 2;
      control.bind_server_epoch (server.server_epoch ());
      auto fixture_request = [&] (QString request_id, qint64 frequency_hz) {
        JtdxWebControl::Request request;
        request.request_id = std::move (request_id);
        request.server_epoch = server.server_epoch ();
        request.state_revision = observed.state_revision;
        request.frequency_hz = frequency_hz;
        request.operation = JtdxWebControl::Operation::Frequency;
        return control.submit (std::move (request));
      };
      check (fixture_request (QStringLiteral ("fixture-completed"), 14075000).status
                 == JtdxWebControl::Status::Completed,
             "browser fixture includes a completed operation");
      check (fixture_request (QStringLiteral ("fixture-failed"), 14076000).status
                 == JtdxWebControl::Status::Failed,
             "browser fixture includes a failed operation");
      check (fixture_request (QStringLiteral ("fixture-timeout"), 14077000).status
                 == JtdxWebControl::Status::Pending,
             "browser fixture admits a timeout operation before expiry");
      control.advance_clock_for_test (1001);
      check (control.expire () && control.result (QStringLiteral ("fixture-timeout")).status
                 == JtdxWebControl::Status::Timeout,
             "browser fixture expires a queued operation without dispatch");
      check (fixture_request (QStringLiteral ("fixture-pending"), 14078000).status
                 == JtdxWebControl::Status::Pending,
             "browser fixture leaves a pending operation for the UI");
      QByteArray fixture_url = server.url ().toUtf8 () + QByteArrayLiteral ("/#fixture");
      std::fprintf (stdout, "WEB_UI_FIXTURE_URL=%s\nWEB_UI_FIXTURE_TOKEN=%s\n",
                    fixture_url.constData (), token.constData ());
      std::fflush (stdout);
      QTimer::singleShot (7000, &state, [&state] { state.advance_clock_for_test (7000); });
      QTimer::singleShot (300000, &app, &QCoreApplication::quit);
      return app.exec ();
    }
  check (!server.is_listening (), "server must be lazy and stopped by default");
  check (server.start (config), "automatic loopback server should start");
  control.bind_server_epoch (server.server_epoch ());
  check (server.is_listening () && server.actual_port () >= JtdxWebServer::automatic_port_first
             && server.actual_port () <= JtdxWebServer::automatic_port_last,
         "automatic port must be bounded");
  quint16 const port = server.actual_port ();
  QByteArray const frequency_body = QByteArrayLiteral ("{\"request_id\":\"tcp-frequency-1\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"frequency_hz\":\"14075000\"}");
  QByteArray response = post_frequency (port, token, frequency_body, QByteArrayLiteral ("http://127.0.0.1:")
                             + QByteArray::number (port), true);
  QJsonDocument frequency_response = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  check (status (response) == 202 && frequency_response.object ().value ("status").toString () == "pending",
         "fragmented TCP frequency POST returns pending admission");
  check (captured_dispatch.request_id == "tcp-frequency-1" && captured_dispatch.frequency_hz == 14075000,
         "TCP frequency POST reaches the production Control dispatch payload");
  check (control.result ("tcp-frequency-1").status == JtdxWebControl::Status::Pending,
         "accepted/pending does not claim completed before CAT feedback");
  JtdxWebControl::Dispatch const dispatch_before_rejections = captured_dispatch;
  auto response_json = [] (QByteArray const& raw) {
    return QJsonDocument::fromJson (raw.mid (raw.indexOf ("\r\n\r\n") + 4)).object ();
  };
  QByteArray invalid_frequency_body = QByteArrayLiteral ("{\"request_id\":\"  invalid-frequency  \",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1,\"frequency_hz\":\"not-a-frequency\"}");
  QJsonObject invalid_frequency_response = response_json (post_frequency (port, token, invalid_frequency_body,
                                                                            QByteArrayLiteral ("http://127.0.0.1:")
                                                                              + QByteArray::number (port)));
  check (invalid_frequency_response.value ("reason").toString () == "invalid_frequency_hz"
             && invalid_frequency_response.value ("request_id").toString () == "invalid-frequency",
         "invalid frequency rejection preserves the trimmed request id");
  QByteArray invalid_revision_body = QByteArrayLiteral ("{\"request_id\":\"  invalid-revision  \",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1.5,\"frequency_hz\":\"14075000\"}");
  QJsonObject invalid_revision_response = response_json (post_frequency (port, token, invalid_revision_body,
                                                                           QByteArrayLiteral ("http://127.0.0.1:")
                                                                             + QByteArray::number (port)));
  check (invalid_revision_response.value ("reason").toString () == "invalid_state_revision"
             && invalid_revision_response.value ("request_id").toString () == "invalid-revision",
         "invalid state revision rejection preserves the trimmed request id");
  QByteArray unknown_field_body = QByteArrayLiteral ("{\"request_id\":\"  unknown-field  \",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1,\"frequency_hz\":\"14075000\",\"extra\":true}");
  QJsonObject unknown_field_response = response_json (post_frequency (port, token, unknown_field_body,
                                                                        QByteArrayLiteral ("http://127.0.0.1:")
                                                                          + QByteArray::number (port)));
  check (unknown_field_response.value ("reason").toString () == "unknown_json_field"
             && unknown_field_response.value ("request_id").toString () == "unknown-field",
         "unknown field rejection preserves the trimmed request id");
  QByteArray invalid_id_body = QByteArrayLiteral ("{\"request_id\":\"bad\\nid\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1,\"frequency_hz\":\"14075000\"}");
  QJsonObject invalid_id_response = response_json (post_frequency (port, token, invalid_id_body,
                                                                     QByteArrayLiteral ("http://127.0.0.1:")
                                                                       + QByteArray::number (port)));
  QString const generated_request_id = invalid_id_response.value ("request_id").toString ();
  check (invalid_id_response.value ("reason").toString () == "invalid_request_id"
             && !generated_request_id.isEmpty ()
             && generated_request_id != QStringLiteral ("bad\nid")
             && JtdxWebControl::normalize_request_id (generated_request_id) == generated_request_id,
         "invalid request id rejection generates a fresh normalized id");
  check (captured_dispatch.request_id == dispatch_before_rejections.request_id
             && captured_dispatch.frequency_hz == dispatch_before_rejections.frequency_hz,
         "business rejections do not invoke the frequency dispatcher");
  QByteArray conflict_body = frequency_body;
  conflict_body.replace ("14075000", "14076000");
  check (status (post_frequency (port, token, conflict_body,
                                QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port))) == 409,
         "same request id with a different payload is rejected as conflict");
  check (status (post_frequency (port, {}, frequency_body,
                                QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port))) == 401,
         "frequency POST requires bearer authentication");
  check (status (post_frequency (port, token, frequency_body, QByteArrayLiteral ("http://evil.example"))) == 403,
         "frequency POST rejects an incorrect Origin");
  check (control.feedback_frequency (captured_dispatch.request_id, captured_dispatch.server_epoch,
                                     captured_dispatch.expected_generation + 1, 14075000,
                                     observed.state_revision + 1),
         "isolated CAT feedback completes the admitted frequency operation");
  response = request (port, QByteArrayLiteral ("/api/v1/state"), token);
  QJsonDocument operations_state = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  QJsonArray operation_rows = operations_state.object ().value (QStringLiteral ("operations")).toArray ();
  check (status (response) == 200 && !operation_rows.isEmpty (),
         "state GET exposes a bounded operations summary");
  QJsonObject completed_operation;
  for (QJsonValue const& row : operation_rows)
    if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("tcp-frequency-1"))
      completed_operation = row.toObject ();
  check (completed_operation.value (QStringLiteral ("status")).toString () == QStringLiteral ("completed")
             && completed_operation.value (QStringLiteral ("reason")).toString () == QStringLiteral ("feedback_matched")
             && completed_operation.value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("confirmed")).toBool (),
         "state operations readback reports completed feedback with confirmation");
  check (!completed_operation.contains (QStringLiteral ("current_state")),
         "operation summary must not recursively embed current_state");

  QByteArray const dx_body = QByteArrayLiteral ("{\"request_id\":\"tcp-dx-1\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"decode_id\":1}");
  QJsonObject const dx_response = response_json (post_select_dx (
      port, token, dx_body, QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (dx_response.value (QStringLiteral ("status")).toString () == QStringLiteral ("pending")
             && captured_dx_dispatch.dx_call == QStringLiteral ("K1ABC")
             && captured_dx_dispatch.dx_grid == QStringLiteral ("FN31")
             && captured_dx_dispatch.dx_selection_source == QStringLiteral ("decode")
             && captured_dx_dispatch.dx_source_decode_id == 1,
         "TCP DX POST resolves a fresh decode into a bounded dispatch payload");
  check (state.json_snapshot ().value (QStringLiteral ("dx_call")).toString () == QStringLiteral ("K1ABC")
             && state.json_snapshot ().value (QStringLiteral ("dx_grid")).toString () == QStringLiteral ("FN31"),
         "DX selection admission does not mutate business state");
  check (control.feedback_select_dx (captured_dx_dispatch.request_id, captured_dx_dispatch.server_epoch,
                                     captured_dx_dispatch.expected_generation + 1,
                                     captured_dx_dispatch.dx_call, captured_dx_dispatch.dx_grid,
                                     observed.state_revision + 1, {},
                                     captured_dx_dispatch.dx_frequency_offset,
                                     captured_dx_dispatch.dx_time,
                                     captured_dx_dispatch.dx_selection_source,
                                     captured_dx_dispatch.dx_source_decode_id),
         "isolated DX feedback completes the admitted selection operation");
  QJsonObject const completed_dx = response_json (post_select_dx (
      port, token, dx_body, QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (completed_dx.value (QStringLiteral ("status")).toString () == QStringLiteral ("completed")
             && completed_dx.value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("dx_call")).toString ()
                    == QStringLiteral ("K1ABC")
             && completed_dx.value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("dx_source_decode_id")).toInt () == 1,
         "completed DX result exposes matching readback metadata");
  QByteArray const stale_dx_body = QByteArrayLiteral ("{\"request_id\":\"tcp-dx-stale\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"decode_id\":999999}");
  check (status (post_select_dx (port, token, stale_dx_body,
                                QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port))) == 409,
         "stale or unknown decode id is rejected before dispatch");

  JtdxWebControl::Dispatch captured_business;
  control.set_business_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      captured_business = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "TCP CQ request reaches production prepare dispatcher");
      check (control.begin_dispatch (prepared),
             "TCP CQ request reaches production begin dispatcher");
    });
  QByteArray const cq_body = QByteArrayLiteral ("{\"request_id\":\"tcp-cq-1\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"confirm\":true}");
  QJsonObject const cq_response = response_json (post_business (
      port, token, QByteArrayLiteral ("start-cq"), cq_body,
      QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (cq_response.value (QStringLiteral ("status")).toString () == QStringLiteral ("pending")
             && captured_business.operation == JtdxWebControl::Operation::StartCq,
         "confirmed CQ POST enters the business operation queue");
  auto business_observation = observed;
  business_observation.business_generation = 1;
  business_observation.business_state_known = true;
  business_observation.cq_state = QStringLiteral ("armed");
  business_observation.state_revision = observed.state_revision + 1;
  control.set_observed_state (business_observation);
  check (control.feedback_business (captured_business.request_id, captured_business.server_epoch,
                                    business_observation.business_generation,
                                    business_observation.cq_state, false,
                                    business_observation.state_revision),
         "CQ operation completes only on matching business state feedback");
  QJsonObject const completed_cq = response_json (post_business (
      port, token, QByteArrayLiteral ("start-cq"), cq_body,
      QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (completed_cq.value (QStringLiteral ("status")).toString () == QStringLiteral ("completed")
             && completed_cq.value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("cq_state")).toString ()
                    == QStringLiteral ("armed"),
         "completed CQ result exposes confirmed business readback");

  auto active_business = observed;
  active_business.state_revision += 2;
  active_business.business_generation = 4;
  active_business.business_state_known = true;
  active_business.auto_sequence_enabled = true;
  active_business.cq_state = QStringLiteral ("transmitting");
  active_business.safety.tx_enabled = true;
  active_business.safety.transmitting = true;
  active_business.safety.ptt = true;
  active_business.safety.watchdog_timeout = true;
  control.set_observed_state (active_business);
  QByteArray const stop_body = QByteArrayLiteral ("{\"request_id\":\"tcp-stop-active\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (active_business.state_revision) + QByteArrayLiteral (",\"confirm\":true}");
  QJsonObject const stop_response = response_json (post_business (
      port, token, QByteArrayLiteral ("stop-auto-call"), stop_body,
      QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (stop_response.value (QStringLiteral ("status")).toString () == QStringLiteral ("pending")
             && captured_business.operation == JtdxWebControl::Operation::StopAutoCall,
         "active TX/PTT does not block the high-priority stop route");
  check (control.feedback_business (captured_business.request_id, captured_business.server_epoch,
                                    active_business.business_generation + 1, QStringLiteral ("idle"), false,
                                    active_business.state_revision + 1),
         "active stop completes only after safe idle business readback");
  QJsonObject const completed_stop = response_json (post_business (
      port, token, QByteArrayLiteral ("stop-auto-call"), stop_body,
      QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port)));
  check (completed_stop.value (QStringLiteral ("status")).toString () == QStringLiteral ("completed")
             && completed_stop.value (QStringLiteral ("reason")).toString () == QStringLiteral ("automation_stopped"),
         "active stop exposes an explicit automation-stopped result");
  control.set_observed_state (observed);

  // A live SSE peer must receive a new snapshot when only the Control result changes.
  QTcpSocket operation_sse;
  QEventLoop operation_sse_loop;
  QTimer operation_sse_timer;
  QByteArray operation_sse_response;
  bool initial_sse_complete = false;
  operation_sse_timer.setSingleShot (true);
  QObject::connect (&operation_sse, &QTcpSocket::readyRead, [&] {
      operation_sse_response += operation_sse.readAll ();
      if ((!initial_sse_complete && operation_sse_response.contains (QByteArrayLiteral ("event: snapshot\n")))
          || (initial_sse_complete && operation_sse_response.contains (QByteArrayLiteral ("sse-frequency"))
              && operation_sse_response.contains (QByteArrayLiteral ("\"status\":\"completed\""))))
        operation_sse_loop.quit ();
    });
  QObject::connect (&operation_sse_timer, &QTimer::timeout, &operation_sse_loop, &QEventLoop::quit);
  operation_sse.connectToHost (QHostAddress::LocalHost, port);
  check (operation_sse.waitForConnected (1000), "operation SSE fixture must connect");
  operation_sse.write (QByteArrayLiteral ("GET /api/v1/events HTTP/1.1\r\nHost: 127.0.0.1:")
                       + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ")
                       + token + QByteArrayLiteral ("\r\n\r\n"));
  operation_sse.flush ();
  operation_sse_timer.start (2000);
  operation_sse_loop.exec ();
  QByteArray const initial_operation_sse = operation_sse_response;
  initial_sse_complete = true;
  QVector<ParsedSseEvent> initial_events = parse_sse_events (initial_operation_sse);
  QByteArray initial_snapshot_id;
  for (ParsedSseEvent const& event : initial_events)
    if (event.name == QByteArrayLiteral ("snapshot") && event.data.contains (QStringLiteral ("operations")))
      initial_snapshot_id = event.id;
  check (!initial_snapshot_id.isEmpty (), "operation SSE initial snapshot must succeed");
  QByteArray const sse_body = QByteArrayLiteral ("{\"request_id\":\"sse-frequency\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"frequency_hz\":\"14076000\"}");
  QByteArray const sse_post = post_frequency (port, token, sse_body,
                                               QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port));
  check (status (sse_post) == 202 && control.result (QStringLiteral ("sse-frequency")).status
             == JtdxWebControl::Status::Pending,
         "SSE operation fixture remains pending before feedback");
  check (control.feedback_frequency (captured_dispatch.request_id, captured_dispatch.server_epoch,
                                     captured_dispatch.expected_generation + 1, 14076000,
                                     observed.state_revision + 2),
         "SSE operation fixture accepts isolated completion feedback");
  operation_sse_timer.start (3000);
  operation_sse_loop.exec ();
  QByteArray completed_sse_id;
  bool sse_completed = false;
  for (ParsedSseEvent const& event : parse_sse_events (operation_sse_response))
    if (event.name == QByteArrayLiteral ("snapshot"))
      for (QJsonValue const& row : event.data.value (QStringLiteral ("operations")).toArray ())
        if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("sse-frequency")
            && row.toObject ().value (QStringLiteral ("status")).toString () == QStringLiteral ("completed")
            && row.toObject ().value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("confirmed")).toBool ())
          {
            sse_completed = true;
            completed_sse_id = event.id;
          }
  check (sse_completed && completed_sse_id != initial_snapshot_id,
         "SSE snapshot updates with a fresh id when only operation status changes");
  operation_sse.disconnectFromHost ();

  // Timeout is observable through the same state snapshot and does not call CAT feedback.
  JtdxWebControl timeout_control {40};
  timeout_control.set_observed_state (observed);
  JtdxWebControl::Dispatch timeout_dispatch;
  timeout_control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      timeout_dispatch = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (timeout_control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "timeout fixture prepares through Control");
      check (timeout_control.begin_dispatch (prepared), "timeout fixture begins through Control");
    });
  server.set_control (&timeout_control);
  timeout_control.bind_server_epoch (server.server_epoch ());
  QTcpSocket timeout_sse;
  QEventLoop timeout_sse_loop;
  QTimer timeout_sse_timer;
  QByteArray timeout_sse_response;
  bool timeout_sse_initial = false;
  timeout_sse_timer.setSingleShot (true);
  QObject::connect (&timeout_sse, &QTcpSocket::readyRead, [&] {
      timeout_sse_response += timeout_sse.readAll ();
      if (!timeout_sse_initial && timeout_sse_response.contains (QByteArrayLiteral ("event: snapshot\n")))
        timeout_sse_loop.quit ();
      if (timeout_sse_initial)
        for (ParsedSseEvent const& event : parse_sse_events (timeout_sse_response))
          if (event.name == QByteArrayLiteral ("snapshot"))
            for (QJsonValue const& row : event.data.value (QStringLiteral ("operations")).toArray ())
              if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("tcp-timeout")
                  && row.toObject ().value (QStringLiteral ("status")).toString () == QStringLiteral ("timeout"))
                timeout_sse_loop.quit ();
    });
  QObject::connect (&timeout_sse_timer, &QTimer::timeout, &timeout_sse_loop, &QEventLoop::quit);
  timeout_sse.connectToHost (QHostAddress::LocalHost, port);
  check (timeout_sse.waitForConnected (1000), "timeout SSE fixture must connect");
  timeout_sse.write (QByteArrayLiteral ("GET /api/v1/events HTTP/1.1\r\nHost: 127.0.0.1:")
                     + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ")
                     + token + QByteArrayLiteral ("\r\n\r\n"));
  timeout_sse.flush ();
  timeout_sse_timer.start (2000);
  timeout_sse_loop.exec ();
  QByteArray timeout_initial_id;
  for (ParsedSseEvent const& event : parse_sse_events (timeout_sse_response))
    if (event.name == QByteArrayLiteral ("snapshot")) timeout_initial_id = event.id;
  timeout_sse_initial = true;
  check (!timeout_initial_id.isEmpty (), "timeout SSE initial snapshot must succeed");
  QByteArray const timeout_body = QByteArrayLiteral ("{\"request_id\":\"tcp-timeout\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"frequency_hz\":\"14077000\"}");
  check (status (post_frequency (port, token, timeout_body,
                                 QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port))) == 202,
         "timeout fixture is admitted as pending");
  QEventLoop timeout_loop;
  QTimer timeout_wait;
  timeout_wait.setSingleShot (true);
  QObject::connect (&timeout_wait, &QTimer::timeout, &timeout_loop, &QEventLoop::quit);
  timeout_wait.start (100);
  timeout_loop.exec ();
  timeout_sse_timer.start (3000);
  timeout_sse_loop.exec ();
  QByteArray timeout_event_id;
  bool timeout_sse_seen = false;
  for (ParsedSseEvent const& event : parse_sse_events (timeout_sse_response))
    if (event.name == QByteArrayLiteral ("snapshot"))
      for (QJsonValue const& row : event.data.value (QStringLiteral ("operations")).toArray ())
        if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("tcp-timeout")
            && row.toObject ().value (QStringLiteral ("status")).toString () == QStringLiteral ("timeout"))
          {
            timeout_sse_seen = true;
            timeout_event_id = event.id;
          }
  check (timeout_sse_seen && timeout_event_id != timeout_initial_id,
         "timeout updates active SSE without CAT feedback state change");
  response = request (port, QByteArrayLiteral ("/api/v1/state"), token);
  QJsonDocument timeout_state = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  bool timeout_seen = false;
  for (QJsonValue const& row : timeout_state.object ().value (QStringLiteral ("operations")).toArray ())
    if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("tcp-timeout"))
      timeout_seen = row.toObject ().value (QStringLiteral ("status")).toString () == QStringLiteral ("timeout")
          && !row.toObject ().value (QStringLiteral ("readback")).toObject ().value (QStringLiteral ("confirmed")).toBool ();
  check (timeout_seen, "timeout status and unconfirmed readback are visible over state GET");
  check (timeout_control.result (QStringLiteral ("tcp-timeout")).snapshot.frequency_generation
             == observed.frequency_generation,
         "timeout leaves the isolated CAT observation generation unchanged");
  timeout_sse.disconnectFromHost ();
  response = sse_request (port, token, timeout_event_id, 2000);
  bool timeout_resync_seen = false;
  for (ParsedSseEvent const& event : parse_sse_events (response))
    if (event.name == QByteArrayLiteral ("snapshot"))
      for (QJsonValue const& row : event.data.value (QStringLiteral ("operations")).toArray ())
        if (row.toObject ().value (QStringLiteral ("request_id")).toString () == QStringLiteral ("tcp-timeout")
            && row.toObject ().value (QStringLiteral ("status")).toString () == QStringLiteral ("timeout"))
          timeout_resync_seen = true;
  check (response.contains (QByteArrayLiteral ("event: resync_required\n")) && timeout_resync_seen,
         "Last-Event-ID reconnect resynchronizes the bounded timeout result");
  // The timeout latch is service-wide: another browser/client must not replay
  // an operation merely because it has a different request id or SSE session.
  QByteArray const cross_client_body = QByteArrayLiteral ("{\"request_id\":\"browser-two\",\"server_epoch\":\"")
      + server.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":")
      + QByteArray::number (observed.state_revision) + QByteArrayLiteral (",\"frequency_hz\":\"14078000\"}");
  QByteArray const cross_client_response = post_frequency (
      port, token, cross_client_body,
      QByteArrayLiteral ("http://127.0.0.1:") + QByteArray::number (port));
  QJsonDocument cross_client_json = QJsonDocument::fromJson (
      cross_client_response.mid (cross_client_response.indexOf ("\r\n\r\n") + 4));
  check (status (cross_client_response) == 409
             && cross_client_json.object ().value (QStringLiteral ("reason")).toString ()
                    == QStringLiteral ("unconfirmed_feedback"),
         "a second browser cannot replay frequency control after an unconfirmed timeout");
  {
    JtdxWebServer no_control {&state};
    auto no_control_config = config;
    no_control_config.automatic_port = true;
    no_control.set_frequency_validator ([&] (QString const& input) {
        return JtdxWebFrequency::parse_and_validate_hz (input, bands);
      });
    check (no_control.start (no_control_config), "no-Control fixture starts");
    QByteArray no_control_body = QByteArrayLiteral ("{\"request_id\":\"  no-control  \",\"server_epoch\":\"")
        + no_control.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1,\"frequency_hz\":\"14075000\"}");
    QByteArray no_control_response = post_frequency (no_control.actual_port (), token, no_control_body,
                                                     QByteArrayLiteral ("http://127.0.0.1:")
                                                       + QByteArray::number (no_control.actual_port ()));
    QJsonDocument no_control_json = QJsonDocument::fromJson (no_control_response.mid (no_control_response.indexOf ("\r\n\r\n") + 4));
    check (status (no_control_response) == 409
               && no_control_json.object ().value ("reason").toString () == "control_unavailable"
               && no_control_json.object ().value ("request_id").toString () == "no-control",
           "frequency control without a bound Control is rejected by default");
    no_control_response = request (no_control.actual_port (), QByteArrayLiteral ("/api/v1/state"), token);
    no_control_json = QJsonDocument::fromJson (no_control_response.mid (no_control_response.indexOf ("\r\n\r\n") + 4));
    check (no_control_json.object ().value (QStringLiteral ("frequency_control_enabled")).isBool ()
               && !no_control_json.object ().value (QStringLiteral ("frequency_control_enabled")).toBool (),
           "state must report the frequency control gate disabled without a bound Control");
    no_control.stop ();
    JtdxWebControl disabled_control {1000};
    disabled_control.set_observed_state (observed);
    no_control.set_control (&disabled_control);
    no_control_config.enable_frequency_control = false;
    check (no_control.start (no_control_config), "disabled frequency gate fixture starts");
    disabled_control.bind_server_epoch (no_control.server_epoch ());
    no_control_response = request (no_control.actual_port (), QByteArrayLiteral ("/api/v1/state"), token);
    no_control_json = QJsonDocument::fromJson (no_control_response.mid (no_control_response.indexOf ("\r\n\r\n") + 4));
    check (no_control_json.object ().value (QStringLiteral ("frequency_control_enabled")).isBool ()
               && !no_control_json.object ().value (QStringLiteral ("frequency_control_enabled")).toBool (),
           "state must report the frequency control gate disabled by configuration with Control bound");
    no_control_body = QByteArrayLiteral ("{\"request_id\":\"  disabled-control  \",\"server_epoch\":\"")
        + no_control.server_epoch ().toUtf8 () + QByteArrayLiteral ("\",\"state_revision\":1,\"frequency_hz\":\"14075000\"}");
    no_control_response = post_frequency (no_control.actual_port (), token, no_control_body,
                                           QByteArrayLiteral ("http://127.0.0.1:")
                                             + QByteArray::number (no_control.actual_port ()));
    no_control_json = QJsonDocument::fromJson (no_control_response.mid (no_control_response.indexOf ("\r\n\r\n") + 4));
    check (status (no_control_response) == 409
               && no_control_json.object ().value ("reason").toString () == "frequency_control_disabled"
               && no_control_json.object ().value ("request_id").toString () == "disabled-control",
           "frequency control remains closed when its explicit gate is disabled");
    no_control.stop ();
  }
  response = request (port, QByteArrayLiteral ("/"));
  check (status (response) == 200 && response.contains (QByteArrayLiteral ("JTDX Web UI")), "root must serve the read-only page");
  check (status (request (port, QByteArrayLiteral ("/style.css"))) == 200, "stylesheet resource must be available without API token");
  check (status (request (port, QByteArrayLiteral ("/app.js"))) == 200, "javascript resource must be available without API token");
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
  check (state_document.object ().value (QStringLiteral ("frequency_control_enabled")).isBool ()
             && state_document.object ().value (QStringLiteral ("frequency_control_enabled")).toBool (),
         "state must report the enabled frequency control gate only after Control epoch binding");
  check (status (request (port, QByteArrayLiteral ("/api/v1/control/frequency"), token,
                         QByteArrayLiteral ("Content-Length: 0\r\n"))) == 400,
         "body framing must be rejected before any control path");
  quint64 const revision_before_post = state.revision ();
  QByteArray const post = QByteArrayLiteral ("POST /api/v1/control/frequency HTTP/1.1\r\nHost: 127.0.0.1:")
      + QByteArray::number (port) + QByteArrayLiteral ("\r\nAuthorization: Bearer ") + token
      + QByteArrayLiteral ("\r\nOrigin: http://127.0.0.1:") + QByteArray::number (port)
      + QByteArrayLiteral ("\r\nContent-Type: application/json\r\n\r\n");
  int const post_status = status (raw_request (port, post));
  check (post_status == 400,
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
  response = request (server.actual_port (), QByteArrayLiteral ("/api/v1/state"), token);
  QJsonDocument restarted_state = QJsonDocument::fromJson (response.mid (response.indexOf ("\r\n\r\n") + 4));
  check (restarted_state.object ().value (QStringLiteral ("operations")).toArray ().isEmpty (),
         "server epoch restart filters old Control results from the new snapshot");
  server.stop ();

  QTcpServer manual_probe;
  check (manual_probe.listen (QHostAddress::LocalHost, 0), "manual port probe should bind");
  quint16 const manual_port = manual_probe.serverPort ();
  manual_probe.close ();
  JtdxWebServer manual_server {&state};
  JtdxWebServer::Configuration manual_ok;
  manual_ok.bearer_token_sha256 = config.bearer_token_sha256;
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
      auto_skip.bearer_token_sha256 = config.bearer_token_sha256;
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
  manual.bearer_token_sha256 = config.bearer_token_sha256;
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
