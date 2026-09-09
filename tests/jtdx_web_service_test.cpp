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
}

#include "jtdx_web_service_test.moc"

int main (int argc, char ** argv)
{
  QGuiApplication application {argc, argv};
  JtdxWebState state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("service-test")};
  JtdxWebService service {&state};
  QAction open_action {QStringLiteral ("打开 Web UI")};
  QObject::connect (&open_action, &QAction::triggered, &service, &JtdxWebService::open);

  JtdxWebServer::Configuration configuration;
  configuration.bearer_token_sha256 = JtdxWebServer::bearer_token_digest (
      QStringLiteral ("p3-service-test-token-0123456789"));

  check (service.apply (false, configuration), "disabled configuration is accepted");
  check (!service.is_listening (), "disabled configuration keeps the server stopped");
  open_action.trigger ();
  check (!service.is_listening (), "open does not start a disabled service");

  UrlHandler handler;
  QDesktopServices::setUrlHandler (QStringLiteral ("http"), &handler, "capture");
  check (service.apply (true, configuration), "enabled loopback configuration starts");
  check (service.is_listening (), "enabled service listens");
  QString const first_epoch = service.server_epoch ();
  quint16 const first_port = service.actual_port ();
  check (!first_epoch.isEmpty () && first_port != 0, "started service exposes epoch and port");

  service.set_url_opener ([] (QUrl const&) { return false; });
  open_action.trigger ();
  check (handler.calls == 0 && service.last_error ().contains (QStringLiteral ("无法打开")),
         "failed URL opener reports an error");
  service.set_url_opener ({});
  open_action.trigger ();
  open_action.trigger ();
  check (handler.calls == 2, "real QAction path opens the production service twice");
  check (handler.last_url.toString () == service.url (), "URL handler receives the service URL");
  check (service.server_epoch () == first_epoch && service.actual_port () == first_port,
         "repeated open keeps the same epoch and port");

  check (service.apply (true, configuration), "same configuration reapplies successfully");
  check (service.server_epoch () == first_epoch && service.actual_port () == first_port,
         "same configuration does not restart the server");

  service.stop ();
  check (!service.is_listening (), "stop closes the listener");
  check (service.apply (true, configuration), "apply after stop recovers the service");
  check (service.is_listening () && service.server_epoch () != first_epoch,
         "restart after stop creates a new epoch");

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
