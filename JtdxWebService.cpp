#include "JtdxWebService.hpp"

#include <QDesktopServices>
#include <QStringList>

#include <utility>

JtdxWebService::JtdxWebService (JtdxWebState * state, QObject * parent)
  : QObject {parent}
  , server_ {new JtdxWebServer {state, this}}
{
  connect (server_, &JtdxWebServer::lifecycle_changed, this,
           [this] (QString const& epoch, bool listening) {
             if (!control_) return;
             if (listening) control_->bind_server_epoch (epoch);
             else control_->invalidate_server_epoch (QStringLiteral ("server_stopped"));
           });
}

JtdxWebService::~JtdxWebService ()
{
  shutdown ();
}

QString JtdxWebService::signature (JtdxWebServer::Configuration const& configuration) const
{
  QStringList udp_ports;
  for (auto const port : configuration.udp_ports) udp_ports << QString::number (port);
  udp_ports.sort ();
  QString bearer_token_digest = configuration.bearer_token_sha256.trimmed ();
  if (bearer_token_digest.isEmpty () && !configuration.bearer_token.isEmpty ())
    bearer_token_digest = JtdxWebServer::bearer_token_digest (configuration.bearer_token);
  return QStringList {
    QString::number (configuration.automatic_port), QString::number (configuration.port),
    configuration.bind_address.toString (), QString::number (configuration.allow_lan),
    bearer_token_digest, configuration.allowed_origin,
    udp_ports.join (QStringLiteral (","))}.join (QChar {'|'});
}

bool JtdxWebService::apply (bool enabled, JtdxWebServer::Configuration const& configuration)
{
  if (shutdown_)
    {
      service_error_ = QStringLiteral ("Web UI 服务已关闭");
      return false;
    }
  if (!enabled)
    {
      stop ();
      return true;
    }

  QString const new_signature = signature (configuration);
  if (server_->is_listening () && new_signature == configuration_signature_)
    {
      service_error_.clear ();
      return true;
    }

  server_->stop ();
  configuration_signature_.clear ();
  service_error_.clear ();
  if (server_->start (configuration))
    {
      configuration_signature_ = new_signature;
      return true;
    }

  service_error_ = server_->last_error ();
  return false;
}

bool JtdxWebService::open ()
{
  if (shutdown_ || !server_->is_listening ())
    {
      service_error_ = shutdown_ ? QStringLiteral ("Web UI 服务已关闭")
                                 : QStringLiteral ("Web UI 未运行");
      return false;
    }
  bool const opened = url_opener_ ? url_opener_ (QUrl {server_->url ()})
                                  : QDesktopServices::openUrl (QUrl {server_->url ()});
  if (!opened)
    {
      service_error_ = QStringLiteral ("无法打开 Web UI URL：%1").arg (server_->url ());
    }
  else service_error_.clear ();
  return opened;
}

void JtdxWebService::stop ()
{
  if (server_) server_->stop ();
  configuration_signature_.clear ();
  service_error_.clear ();
}

void JtdxWebService::shutdown ()
{
  if (shutdown_) return;
  shutdown_ = true;
  stop ();
}

bool JtdxWebService::is_listening () const
{
  return server_ && server_->is_listening ();
}

QString JtdxWebService::url () const
{
  return server_ ? server_->url () : QString {};
}

QString JtdxWebService::last_error () const
{
  return service_error_;
}

QString JtdxWebService::server_epoch () const
{
  return server_ ? server_->server_epoch () : QString {};
}

quint16 JtdxWebService::actual_port () const
{
  return server_ ? server_->actual_port () : 0;
}

int JtdxWebService::active_connection_count () const
{
  return server_ ? server_->active_connection_count () : 0;
}

void JtdxWebService::set_url_opener (UrlOpener opener)
{
  url_opener_ = std::move (opener);
}

void JtdxWebService::set_control (JtdxWebControl * control)
{
  if (control_ && control_ != control)
    control_->invalidate_server_epoch (QStringLiteral ("control_rebound"));
  control_ = control;
  if (!control_) return;
  if (server_ && server_->is_listening ())
    control_->bind_server_epoch (server_->server_epoch ());
  else
    control_->invalidate_server_epoch (QStringLiteral ("server_unavailable"));
}
