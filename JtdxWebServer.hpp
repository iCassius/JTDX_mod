#ifndef JTDX_WEB_SERVER_HPP
#define JTDX_WEB_SERVER_HPP

#include <QHostAddress>
#include <QHash>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTcpServer>

#include "JtdxWebState.hpp"

class QTcpSocket;
class QTimer;

// P2 只读 HTTP/SSE 服务。对象属于创建它的 Qt 事件循环，默认不监听。
// 它不拥有业务状态，也不发送 UDP、CAT、PTT 或任何控制请求。
class JtdxWebServer : public QObject
{
  Q_OBJECT

public:
  struct Configuration
  {
    QHostAddress bind_address {QHostAddress::LocalHost};
    quint16 port {0};                 // 0 = 自动端口；手动端口必须为 1024..65535
    bool automatic_port {true};
    bool allow_lan {false};           // 非 loopback 绑定必须显式开启
    QSet<quint16> udp_ports;          // 仅排除数字，不执行 UDP bind
    QString bearer_token;             // 空值时生成高熵令牌
    QString allowed_origin;            // LAN 时必须精确匹配；空值拒绝 Origin
  };

  static constexpr quint16 automatic_port_first = 49152;
  static constexpr quint16 automatic_port_last = 49251;
  static constexpr int max_connections = 16;
  static constexpr int max_header_bytes = 16 * 1024;
  static constexpr int max_last_event_id_bytes = 64;
  // Measured with 500 legal decode rows (including long text): 554413 bytes
  // for the full JSON body, so 512 KiB would reject a valid P2 snapshot.
  static constexpr int max_sse_event_bytes = 1024 * 1024;
  static constexpr int max_sse_pending_bytes = 1024 * 1024;

  explicit JtdxWebServer (JtdxWebState * state, QObject * parent = nullptr);
  ~JtdxWebServer () override;

  bool start (Configuration configuration);
  void stop ();
  bool is_listening () const;
  quint16 actual_port () const;
  QHostAddress actual_address () const;
  QString url () const;
  QString last_error () const;
  QString server_epoch () const;
  QString web_server_state () const;
  int active_connection_count () const;
  // 仅供同进程测试夹具读取；令牌永不通过 HTTP、URL 或日志回显。
  QString bearer_token_for_testing () const;

private:
  struct Client;

  void accept_connections ();
  void on_ready_read (QTcpSocket * socket);
  void on_disconnected (QTcpSocket * socket);
  void on_header_timeout (QTcpSocket * socket);
  void on_publish_timer ();
  void remove_client (QTcpSocket * socket);
  void close_client (QTcpSocket * socket);
  void reject_connection (QTcpSocket * socket, int status, QByteArray const& reason);
  void process_request (QTcpSocket * socket, QByteArray const& request);

  bool choose_and_listen (Configuration const& configuration);
  bool listen_on (QHostAddress const& address, quint16 port);
  bool validate_configuration (Configuration const& configuration, QString * error) const;
  bool host_allowed (QByteArray const& host) const;
  bool origin_allowed (QByteArray const& origin) const;
  bool authorized (QHash<QByteArray, QByteArray> const& headers) const;
  QByteArray event_id () const;
  QJsonObject state_snapshot () const;
  QByteArray json_response (QJsonObject const& object) const;
  QByteArray http_response (int status, QByteArray const& reason,
                            QByteArray const& content_type, QByteArray const& body,
                            bool close = true) const;
  QByteArray sse_event (QByteArray const& name, QByteArray const& id,
                        QByteArray const& data) const;
  void send_sse (Client * client, QByteArray const& payload);
  void send_http (QTcpSocket * socket, int status, QByteArray const& reason,
                  QByteArray const& content_type, QByteArray const& body);
  void send_initial_sse (Client * client, QByteArray const& last_event_id);
  void broadcast_snapshot (bool force);
  void pump_sse (Client * client);

  QPointer<JtdxWebState> state_;
  QTcpServer server_;
  QTimer * publish_timer_ {nullptr};
  QHash<QTcpSocket *, Client *> clients_;
  Configuration configuration_;
  QHostAddress actual_address_;
  quint16 actual_port_ {0};
  QString bearer_token_;
  QString server_epoch_;
  QString last_error_;
  QString web_server_state_ {QStringLiteral ("stopped")};
  QElapsedTimer activity_clock_;
  quint64 last_published_revision_ {0};
  qint64 last_snapshot_ms_ {-1};
  qint64 last_heartbeat_ms_ {-1};
};

#endif
