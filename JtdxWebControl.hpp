#ifndef JTDX_WEB_CONTROL_HPP
#define JTDX_WEB_CONTROL_HPP

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>

#include <functional>

// 只负责把已规范化的普通控制请求协调到主 Qt 事件循环。
// 本类不拥有 QWidget，不连接 MainWindow，不直接调用 CAT/PTT/TX。
class JtdxWebControl : public QObject
{
  Q_OBJECT

public:
  enum class Operation { Frequency, SelectDx };
  enum class Status { Received, Accepted, Pending, Completed, Failed, Rejected, Timeout };

  struct SafetySnapshot
  {
    bool known {false};
    bool fresh {false};
    bool transmitting {false};
    bool ptt {false};
    bool tx_enabled {false};
    bool watchdog_timeout {false};
    bool business_state_known {false};
    // MainWindow 业务状态投影；未知时默认关闭，保持 fail-closed。
    bool rig_online {false};
    bool monitoring {false};
    bool start2 {false};
    bool tune {false};
    bool auto_tx {false};
    bool iptt {false};
  };

  struct ObservedState
  {
    SafetySnapshot safety;
    quint64 state_revision {0};
    quint64 frequency_generation {0};
    bool frequency_known {false};
    qint64 actual_frequency_hz {0};
    quint64 dx_generation {0};
    bool dx_known {false};
    QString dx_call;
    QString dx_grid;
  };

  struct Request
  {
    QString request_id;
    Operation operation {Operation::Frequency};
    QString server_epoch;
    quint64 state_revision {0};
    qint64 frequency_hz {0};
    QString dx_call;
    QString dx_grid;
  };

  struct Dispatch
  {
    QString request_id;
    QString server_epoch;
    Operation operation {Operation::Frequency};
    quint64 expected_generation {0};
    qint64 frequency_hz {0};
    QString dx_call;
    QString dx_grid;
  };

  struct Result
  {
    QString request_id;
    Operation operation {Operation::Frequency};
    Status status {Status::Received};
    QString reason;
    QString server_epoch;
    quint64 generation {0};
    int http_status {200};
    qint64 received_ms {-1};
    qint64 deadline_ms {-1};
    qint64 completed_ms {-1};
    quint64 initial_state_revision {0};
    ObservedState snapshot;
    QVector<Status> status_history;
  };

  using Clock = std::function<qint64 ()>;
  using ObservationProvider = std::function<ObservedState ()>;
  using DispatchHandler = std::function<void (Dispatch const&)>;

  explicit JtdxWebControl (qint64 timeout_ms = 3000, Clock clock = {}, QObject * parent = nullptr);

  QString server_epoch () const { return epoch_; }
  bool is_shutdown () const { return shutdown_; }
  int record_count () const { return records_.size (); }
  int hard_record_limit () const { return hard_record_limit_; }
  bool has_pending () const { return !pending_request_id_.isEmpty (); }
  Result result (QString const& request_id) const;

  void set_observed_state (ObservedState state);
  void set_observation_provider (ObservationProvider provider);
  void set_frequency_dispatcher (DispatchHandler handler);
  void set_select_dx_dispatcher (DispatchHandler handler);

  // 将控制请求绑定到当前成功监听的 WebServer epoch。绑定不会重建协调器，
  // 因而不会丢失已 begin 操作的未确认锁；停止期间请求保持 fail-closed。
  void bind_server_epoch (QString server_epoch);
  void invalidate_server_epoch (QString reason = QStringLiteral ("server_stopped"));
  bool server_epoch_bound () const { return server_epoch_bound_; }

  // 排队回调真正执行时重新读取生产状态并生成一次性 dispatch。
  // prepare 不消费；begin_dispatch 只允许同一请求实际开始一次。
  bool prepare_dispatch (QString const& request_id, QString const& server_epoch,
                         Dispatch * prepared);
  bool begin_dispatch (Dispatch const& dispatch);

  Result submit (Request request);
  bool feedback_frequency (QString const& request_id, QString const& server_epoch,
                           quint64 generation, qint64 actual_frequency_hz,
                           quint64 state_revision);
  bool feedback_select_dx (QString const& request_id, QString const& server_epoch,
                           quint64 generation, QString dx_call, QString dx_grid,
                           quint64 state_revision);
  bool fail (QString const& request_id, QString const& server_epoch, QString reason);
  bool expire ();
  bool rotate_epoch ();
  void shutdown ();

  // 测试用单调时钟；生产默认使用 QElapsedTimer。
  void set_clock_for_test (qint64 monotonic_ms);
  void advance_clock_for_test (qint64 elapsed_ms);

  static QString operation_name (Operation operation);
  static QString status_name (Status status);

private:
  struct Record
  {
    Result result;
    QString canonical_payload;
    qint64 received_ms {0};
    qint64 deadline_ms {0};
    quint64 baseline_generation {0};
    quint64 baseline_state_revision {0};
    qint64 frequency_hz {0};
    QString dx_call;
    QString dx_grid;
    bool timed_out {false};
    bool prepared {false};
    bool dispatched {false};
  };

  static QString normalize_request_id (QString const& request_id);
  static bool printable_ascii (QString const& value, int max_length);
  static QString canonical_payload (Request const& request);
  static bool safe_to_dispatch (SafetySnapshot const& safety, QString * reason);

  qint64 now () const;
  ObservedState observation () const;
  Result reject (Request const& request, QString reason, int http_status = 400) const;
  void finish (Record& record, Status status, QString reason, quint64 generation = 0);
  void arm_timer ();
  void on_timer ();

  QElapsedTimer elapsed_clock_;
  Clock clock_;
  bool test_clock_ {false};
  qint64 test_now_ms_ {0};
  qint64 timeout_ms_ {3000};
  QString epoch_;
  bool shutdown_ {false};
  bool server_epoch_bound_ {false};
  // 超时或已 begin 操作被服务停止后保持未确认门，避免迟到 CAT 回读与下一请求混淆；
  // 只有显式的上层恢复策略才能清除此门。
  bool unconfirmed_latch_ {false};
  bool submit_in_progress_ {false};
  const int hard_record_limit_ {128};
  QHash<QString, Record> records_;
  QString pending_request_id_;
  ObservedState observed_;
  ObservationProvider observation_provider_;
  DispatchHandler frequency_dispatcher_;
  DispatchHandler select_dx_dispatcher_;
  QString last_timed_out_request_id_;
  QTimer expiry_timer_;
};

#endif
