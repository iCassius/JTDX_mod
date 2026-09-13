#include "JtdxWebService.hpp"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QHostAddress>
#include <QCoreApplication>
#include <QTcpServer>
#include <QTimer>
#include <QtWidgets/QAction>

#include <cstdlib>
#include <iostream>

namespace
{
  class UrlHandler : public QObject
  {
    Q_OBJECT

  public:
    int calls {0};
    QUrl last_url;

  public slots:
    void capture (QUrl const& url)
    {
      ++calls;
      last_url = url;
    }
  };

  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  JtdxWebControl::ObservedState safe_observation ()
  {
    JtdxWebControl::ObservedState state;
    state.safety.known = true;
    state.safety.fresh = true;
    state.safety.rig_online = true;
    state.safety.monitoring = true;
    state.safety.business_state_known = true;
    state.state_revision = 1;
    state.frequency_generation = 1;
    state.frequency_known = true;
    state.actual_frequency_hz = 14073000;
    return state;
  }
}

#include "jtdx_web_service_test.moc"

int main (int argc, char ** argv)
{
  QGuiApplication application {argc, argv};
  JtdxWebState state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("service-test")};
  JtdxWebService service {&state};
  JtdxWebControl control {1000};
  control.set_observed_state (safe_observation ());
  JtdxWebControl::Dispatch queued_dispatch;
  control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      if (queued_dispatch.request_id.isEmpty ()) queued_dispatch = dispatch;
    });
  service.set_control (&control);
  QAction open_action {QStringLiteral ("打开 Web UI")};
  QObject::connect (&open_action, &QAction::triggered, &service, &JtdxWebService::open);

  JtdxWebServer::Configuration configuration;
  configuration.bearer_token_sha256 = JtdxWebServer::bearer_token_digest (
      QStringLiteral ("p3-service-test-token-0123456789"));

  check (service.apply (false, configuration), "disabled configuration is accepted");
  check (!service.is_listening (), "disabled configuration keeps the server stopped");
  auto disabled_request = JtdxWebControl::Request {};
  disabled_request.request_id = QStringLiteral ("disabled");
  disabled_request.operation = JtdxWebControl::Operation::Frequency;
  disabled_request.server_epoch = control.server_epoch ();
  disabled_request.state_revision = 1;
  disabled_request.frequency_hz = 14074000;
  check (control.submit (disabled_request).reason == QStringLiteral ("server_unavailable"),
         "disabled service does not receive control requests");
  open_action.trigger ();
  check (!service.is_listening (), "open does not start a disabled service");

  UrlHandler handler;
  QDesktopServices::setUrlHandler (QStringLiteral ("http"), &handler, "capture");
  check (service.apply (true, configuration), "enabled loopback configuration starts");
  check (service.is_listening (), "enabled service listens");
  QString const first_epoch = service.server_epoch ();
  quint16 const first_port = service.actual_port ();
  check (!first_epoch.isEmpty () && first_port != 0, "started service exposes epoch and port");
  check (control.server_epoch () == first_epoch && control.server_epoch_bound (),
         "control binds to the successful server epoch");

  auto queued_request = disabled_request;
  queued_request.request_id = QStringLiteral ("queued-before-stop");
  queued_request.server_epoch = first_epoch;
  check (control.submit (queued_request).status == JtdxWebControl::Status::Pending,
         "control request is pending before service stop");
  service.stop ();
  check (!control.has_pending () && !control.server_epoch_bound (),
         "service stop invalidates the pending control request");
  check (!control.prepare_dispatch (queued_dispatch.request_id, queued_dispatch.server_epoch,
                                    &queued_dispatch),
         "queued dispatch from stopped service cannot execute");
  check (service.apply (true, configuration), "service restarts after queued request stop");
  QString const running_epoch = service.server_epoch ();
  quint16 const running_port = service.actual_port ();
  check (control.server_epoch () == running_epoch && control.server_epoch () != first_epoch,
         "restart binds control to a new server epoch");

  service.set_url_opener ([] (QUrl const&) { return false; });
  open_action.trigger ();
  check (handler.calls == 0 && service.last_error ().contains (QStringLiteral ("无法打开")),
         "failed URL opener reports an error");
  service.set_url_opener ({});
  open_action.trigger ();
  open_action.trigger ();
  check (handler.calls == 2, "real QAction path opens the production service twice");
  check (handler.last_url.toString () == service.url (), "URL handler receives the service URL");
  check (service.server_epoch () == running_epoch && service.actual_port () == running_port,
         "repeated open keeps the same epoch and port");

  check (service.apply (true, configuration), "same configuration reapplies successfully");
  check (service.server_epoch () == running_epoch && service.actual_port () == running_port,
         "same configuration does not restart the server");

  JtdxWebControl::Dispatch begun_dispatch;
  control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      begun_dispatch = dispatch;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "begun request prepares before service stop");
      check (control.begin_dispatch (prepared), "begun request starts before service stop");
    });
  auto begun_request = queued_request;
  begun_request.request_id = QStringLiteral ("begun-before-stop");
  begun_request.server_epoch = running_epoch;
  check (control.submit (begun_request).status == JtdxWebControl::Status::Pending,
         "begun request remains pending until feedback");

  service.stop ();
  check (!service.is_listening (), "stop closes the listener");
  check (service.apply (true, configuration), "apply after stop recovers the service");
  check (service.is_listening () && service.server_epoch () != first_epoch,
         "restart after stop creates a new epoch");
  auto post_restart_request = begun_request;
  post_restart_request.request_id = QStringLiteral ("new-after-begun-stop");
  post_restart_request.server_epoch = service.server_epoch ();
  check (control.submit (post_restart_request).reason == QStringLiteral ("unconfirmed_feedback"),
         "restart does not unlock an unconfirmed begun request");
  check (!control.begin_dispatch (begun_dispatch),
         "begun dispatch from the previous server epoch cannot execute after restart");

  QTcpServer occupied;
  check (occupied.listen (QHostAddress::LocalHost, 0), "occupied port fixture starts");
  JtdxWebServer::Configuration conflict = configuration;
  conflict.automatic_port = false;
  conflict.port = occupied.serverPort ();
  service.stop ();
  check (!service.apply (true, conflict), "occupied port failure is reported");
  check (!service.is_listening () && !service.last_error ().isEmpty (),
         "failed start leaves the service stopped with an error");
  occupied.close ();
  check (service.apply (true, conflict), "failed start recovers after the port is released");
  check (service.is_listening () && service.last_error ().isEmpty (),
         "recovered service listens and clears the previous error");

  QMetaObject::invokeMethod (&open_action, "trigger", Qt::QueuedConnection);
  service.shutdown ();
  check (service.is_shutdown () && !service.is_listening (), "shutdown closes and locks the service");
  int const calls_before_shutdown_open = handler.calls;
  QCoreApplication::processEvents (QEventLoop::AllEvents);
  check (handler.calls == calls_before_shutdown_open && !service.is_listening (),
         "shutdown rejects queued or later open actions");
  check (!service.apply (true, configuration) && !service.is_listening (),
         "shutdown rejects a later apply request");

  QDesktopServices::unsetUrlHandler (QStringLiteral ("http"));

  {
    JtdxWebState repeat_state {QStringLiteral ("JTDX"), QStringLiteral ("test"),
                               QStringLiteral ("repeat-control")};
    JtdxWebService repeat_service {&repeat_state};
    JtdxWebControl repeat_control {1000};
    repeat_control.set_observed_state (safe_observation ());
    repeat_control.set_frequency_dispatcher ([] (JtdxWebControl::Dispatch const&) {});
    repeat_service.set_control (&repeat_control);
    check (repeat_service.apply (true, configuration), "repeat-control fixture starts");
    JtdxWebControl::Request repeat_request;
    repeat_request.request_id = QStringLiteral ("repeat-binding");
    repeat_request.operation = JtdxWebControl::Operation::Frequency;
    repeat_request.server_epoch = repeat_service.server_epoch ();
    repeat_request.state_revision = 1;
    repeat_request.frequency_hz = 14074000;
    check (repeat_control.submit (repeat_request).status == JtdxWebControl::Status::Pending,
           "repeat-control fixture has an in-flight request");
    repeat_service.set_control (&repeat_control);
    check (repeat_control.has_pending (), "rebinding the same Control keeps its pending request");
    QTcpServer occupied_repeat;
    check (occupied_repeat.listen (QHostAddress::LocalHost, 0),
           "repeat-control occupied-port fixture starts");
    auto failed_configuration = configuration;
    failed_configuration.automatic_port = false;
    failed_configuration.port = occupied_repeat.serverPort ();
    check (!repeat_service.apply (true, failed_configuration),
           "failed restart reports the occupied port");
    check (!repeat_control.has_pending () && !repeat_control.server_epoch_bound (),
           "failed restart invalidates the previous pending request");
    occupied_repeat.close ();
    repeat_service.stop ();
  }

  {
    auto *destroyed_state = new JtdxWebState {QStringLiteral ("JTDX"), QStringLiteral ("test"),
                                               QStringLiteral ("state-destroyed")};
    JtdxWebService state_service {destroyed_state};
    JtdxWebControl state_control {1000};
    state_service.set_control (&state_control);
    check (state_service.apply (true, configuration), "state-destroyed fixture starts");
    delete destroyed_state;
    check (!state_service.is_listening () && !state_control.server_epoch_bound (),
           "State destruction stops service and unbinds control");
  }

  {
    JtdxWebState control_state {QStringLiteral ("JTDX"), QStringLiteral ("test"),
                                QStringLiteral ("control-destroyed")};
    JtdxWebService control_service {&control_state};
    auto *destroyed_control = new JtdxWebControl {1000};
    control_service.set_control (destroyed_control);
    check (control_service.apply (true, configuration), "control-destroyed fixture starts");
    delete destroyed_control;
    control_service.stop ();
    check (!control_service.is_listening (), "service remains safe after Control destruction");
  }

  {
    JtdxWebState service_state {QStringLiteral ("JTDX"), QStringLiteral ("test"),
                                QStringLiteral ("service-destroyed")};
    JtdxWebControl surviving_control {1000};
    {
      JtdxWebService scoped_service {&service_state};
      scoped_service.set_control (&surviving_control);
      check (scoped_service.apply (true, configuration), "service-destroyed fixture starts");
    }
    check (!surviving_control.server_epoch_bound (),
           "service destruction invalidates its surviving Control binding");
  }

  quint16 cleanup_port = 0;
  {
    QTcpServer probe;
    check (probe.listen (QHostAddress::LocalHost, 0), "cleanup port probe starts");
    cleanup_port = probe.serverPort ();
    probe.close ();
    JtdxWebState cleanup_state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("cleanup")};
    JtdxWebService cleanup_service {&cleanup_state};
    JtdxWebServer::Configuration cleanup_configuration = configuration;
    cleanup_configuration.automatic_port = false;
    cleanup_configuration.port = cleanup_port;
    check (cleanup_service.apply (true, cleanup_configuration), "scoped service starts on cleanup port");
  }
  QTcpServer cleanup_probe;
  check (cleanup_probe.listen (QHostAddress::LocalHost, cleanup_port),
         "service destructor releases its TCP port");
  std::cout << "Web service lifecycle checks passed\n";
  return 0;
}
