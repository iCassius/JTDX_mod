#ifndef JTDX_WEB_SERVICE_HPP
#define JTDX_WEB_SERVICE_HPP

#include <QUrl>
#include <QPointer>

#include <functional>

#include "JtdxWebServer.hpp"
#include "JtdxWebControl.hpp"

// P3 Web 生命周期胶水。它只负责把 MainWindow 转换好的配置交给只读
// JtdxWebServer，并提供菜单打开、停止和退出后的关闭门禁；不依赖
// Configuration、控件、CAT、PTT、TX、音频或 UDP。
class JtdxWebService : public QObject
{
public:
  using UrlOpener = std::function<bool (QUrl const&)>;

  explicit JtdxWebService (JtdxWebState * state, QObject * parent = nullptr);
  ~JtdxWebService () override;

  bool apply (bool enabled, JtdxWebServer::Configuration const& configuration);
  bool open ();
  void stop ();
  void shutdown ();

  bool is_listening () const;
  QString url () const;
  QString last_error () const;
  QString server_epoch () const;
  quint16 actual_port () const;
  int active_connection_count () const;
  bool is_shutdown () const { return shutdown_; }

  // 绑定 MainWindow 持有的唯一控制协调器；QPointer 避免独立销毁时悬垂。
  void set_control (JtdxWebControl * control);

  // 仅供 Qt-only 验收替换 URL 打开器；生产默认使用 QDesktopServices。
  void set_url_opener (UrlOpener opener);

private:
  QString signature (JtdxWebServer::Configuration const& configuration) const;
  JtdxWebServer * server_ {nullptr};
  QPointer<JtdxWebControl> control_;
  QString configuration_signature_;
  QString service_error_;
  UrlOpener url_opener_;
  bool shutdown_ {false};
};

#endif
