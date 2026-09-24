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
#include "JtdxWebControl.hpp"
#include "JtdxWebFrequency.hpp"
#include "JtdxWebDx.hpp"
#include <functional>
#include <utility>

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
    QSet<quint16> udp_ports;          // 仅排除数字，不执行 UDP bind
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
  void set_control (JtdxWebControl * control);
  using FrequencyValidator = std::function<JtdxWebFrequency::Result (QString const&)>;
  void set_frequency_validator (FrequencyValidator validator) { frequency_validator_ = std::move (validator); }

Q_SIGNALS:
  // 仅报告 TCP 监听生命周期；控制层由 JtdxWebService 绑定到成功监听的 epoch。
  void lifecycle_changed (QString server_epoch, bool listening);

private:
  struct Client;

  void accept_connections ();
  void on_ready_read (QTcpSocket * socket);
  void on_disconnected (QTcpSocket * socket);
  void on_header_timeout (QTcpSocket * socket);
  void on_publish_timer ();
  void schedule_snapshot_push ();
  void remove_client (QTcpSocket * socket);
  void close_client (QTcpSocket * socket);
  void reject_connection (QTcpSocket * socket, int status, QByteArray const& reason);
  void process_request (QTcpSocket * socket, QByteArray const& request);

  bool choose_and_listen (Configuration const& configuration);
  bool listen_on (QHostAddress const& address, quint16 port);
  bool validate_configuration (Configuration const& configuration, QString * error) const;
  QByteArray event_id () const;
  QJsonObject state_snapshot () const;
  QJsonObject operation_result (JtdxWebControl::Result const& result) const;
  QJsonObject operations_snapshot () const;
  QByteArray json_response (QJsonObject const& object) const;
  QJsonObject control_response (JtdxWebControl::Result const& result) const;
  QJsonObject control_error_response (int status, QString reason, QString request_id = {},
                                      JtdxWebControl::Operation operation = JtdxWebControl::Operation::Frequency) const;
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
  QPointer<JtdxWebControl> control_;
  QMetaObject::Connection control_destroyed_connection_;
  QMetaObject::Connection control_operations_connection_;
  FrequencyValidator frequency_validator_;
  QTcpServer server_;
  QTimer * publish_timer_ {nullptr};
  QTimer * snapshot_push_timer_ {nullptr};
  QHash<QTcpSocket *, Client *> clients_;
  Configuration configuration_;
  QHostAddress actual_address_;
  quint16 actual_port_ {0};
  quint64 control_generation_ {0};
  QString server_epoch_;
  QString last_error_;
  QString web_server_state_ {QStringLiteral ("stopped")};
  QElapsedTimer activity_clock_;
  quint64 last_published_revision_ {0};
  quint64 last_published_operations_revision_ {0};
  qint64 last_snapshot_ms_ {-1};
  qint64 last_heartbeat_ms_ {-1};
};

#endif
