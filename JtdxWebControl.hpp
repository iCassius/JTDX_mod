#ifndef JTDX_WEB_CONTROL_HPP
#define JTDX_WEB_CONTROL_HPP

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>
#include <QStringList>

#include <functional>

// 只负责把已规范化的普通控制请求协调到主 Qt 事件循环。
// 本类不拥有 QWidget，不连接 MainWindow，不直接调用 CAT/PTT/TX。
class JtdxWebControl : public QObject
{
  Q_OBJECT

public:
  enum class Operation { Frequency, SelectDx, StartCq, StartAutoCall, StopAutoCall, Radio };
  enum class Status { Received, Accepted, Pending, Completed, Failed, Rejected, Timeout };

  struct SafetySnapshot
  {
    bool known {false};
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
    QString dx_report;
    qint32 dx_frequency_offset {0};
    QString dx_time;
    QString dx_selection_source;
    quint64 dx_source_decode_id {0};
    quint64 business_generation {0};
    bool business_state_known {false};
    QString cq_state;
    bool auto_sequence_enabled {false};
    bool radio_state_known {false};
    bool radio_multi_decode {false};
    bool radio_agc_compensation {false};
    bool radio_narrow {false};
    bool radio_sync {false};
    bool radio_skip_tx1 {false};
    int radio_current_tx_index {0};
    QStringList radio_tx_messages;
    bool radio_log_dialog_open {false};
    QJsonObject radio_qso_draft;
    quint64 radio_qso_generation {0};
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
    QString dx_report;
    qint32 dx_frequency_offset {0};
    QString dx_time;
    QString dx_selection_source;
    quint64 dx_source_decode_id {0};
    QString radio_action;
    bool radio_value {false};
    int radio_index {0};
    QString radio_text;
    QJsonObject radio_qso;
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
    QString dx_report;
    qint32 dx_frequency_offset {0};
    QString dx_time;
    QString dx_selection_source;
    quint64 dx_source_decode_id {0};
    QString radio_action;
    bool radio_value {false};
    int radio_index {0};
    QString radio_text;
    QJsonObject radio_qso;
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
  using DiagnosticLogger = std::function<void (QString const&)>;

  explicit JtdxWebControl (qint64 timeout_ms = 3000, Clock clock = {}, QObject * parent = nullptr);

  QString server_epoch () const { return epoch_; }
  bool is_shutdown () const { return shutdown_; }
  int record_count () const { return records_.size (); }
  int hard_record_limit () const { return hard_record_limit_; }
  bool has_pending () const { return !pending_request_id_.isEmpty (); }
  Result result (QString const& request_id) const;
  // 返回按接收时间和 request_id 稳定排序的副本；调用方不得持有内部记录引用。
  QVector<Result> operation_results () const;
  quint64 operation_revision () const { return operation_revision_; }

  void set_observed_state (ObservedState state);
  void set_observation_provider (ObservationProvider provider);
  ObservedState observed_state () const;
  void set_frequency_dispatcher (DispatchHandler handler);
  void set_select_dx_dispatcher (DispatchHandler handler);
  void set_business_dispatcher (DispatchHandler handler) { business_dispatcher_ = std::move (handler); }
  void set_diagnostic_logger (DiagnosticLogger logger) { diagnostic_logger_ = std::move (logger); }

Q_SIGNALS:
  void operations_changed (quint64 revision);

public:

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
                           quint64 state_revision, QString dx_report = {},
                           qint32 dx_frequency_offset = 0, QString dx_time = {},
                           QString dx_selection_source = {}, quint64 dx_source_decode_id = 0);
  bool feedback_business (QString const& request_id, QString const& server_epoch,
                          quint64 generation, QString cq_state, bool auto_sequence_enabled,
                          quint64 state_revision);
  bool feedback_radio (QString const& request_id, QString const& server_epoch,
                       quint64 state_revision, ObservedState const& observed);
  bool fail (QString const& request_id, QString const& server_epoch, QString reason);
  bool expire ();
  bool rotate_epoch ();
  void shutdown ();

  // 测试用单调时钟；生产默认使用 QElapsedTimer。
  void set_clock_for_test (qint64 monotonic_ms);
  void advance_clock_for_test (qint64 elapsed_ms);

  static QString operation_name (Operation operation);
  static QString status_name (Status status);
  static QString normalize_request_id (QString const& request_id);

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
    QString dx_report;
    qint32 dx_frequency_offset {0};
    QString dx_time;
    QString dx_selection_source;
    quint64 dx_source_decode_id {0};
    quint64 baseline_business_generation {0};
    QString radio_action;
    bool radio_value {false};
    int radio_index {0};
    QString radio_text;
    QJsonObject radio_qso;
    QString target_cq_state;
    bool target_auto_sequence_enabled {false};
    bool timed_out {false};
    bool prepared {false};
    bool dispatched {false};
  };

  static bool printable_ascii (QString const& value, int max_length);
  static bool printable_radio_text (QString const& value, int max_length);
  static QString canonical_payload (Request const& request);
  static bool safe_to_dispatch (SafetySnapshot const& safety, Operation operation,
                                QString const& radio_action, bool radio_value, QString * reason);
  static bool requires_unconfirmed_latch (Record const& record);
  void mark_operations_changed ();

  qint64 now () const;
  ObservedState observation () const;
  Result reject (Request const& request, QString reason, int http_status = 400);
  void finish (Record& record, Status status, QString reason, quint64 generation = 0);
  void log_result (Result const& result, QString event) const;
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
  // Stop 仍可走既有 fail-safe 停止路径，但不会因此清除此门。
  bool unconfirmed_latch_ {false};
  bool submit_in_progress_ {false};
  quint64 operation_revision_ {0};
  const int hard_record_limit_ {128};
  QHash<QString, Record> records_;
  QString pending_request_id_;
  ObservedState observed_;
  ObservationProvider observation_provider_;
  DispatchHandler frequency_dispatcher_;
  DispatchHandler select_dx_dispatcher_;
  DispatchHandler business_dispatcher_;
  DiagnosticLogger diagnostic_logger_;
  QString last_timed_out_request_id_;
  QTimer expiry_timer_;
};

#endif
