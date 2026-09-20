#ifndef JTDX_WEB_STATE_HPP
#define JTDX_WEB_STATE_HPP

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QSet>
#include <QThread>
#include <QTime>
#include <QString>

#include <utility>

#include "Radio.hpp"

// 只读状态由 JTDX 主 Qt 事件循环线程拥有，不保存 QWidget 指针，也不承担网络或控制职责。
class JtdxWebState
  : public QObject
{
  Q_OBJECT

public:
  using Frequency = Radio::Frequency;
  static constexpr int default_decode_limit = 300;
  static constexpr int hard_decode_limit = 500;
  static constexpr int default_frequency_candidate_limit = 200;
  static constexpr int hard_frequency_candidate_limit = 500;
  static QString project_cq_state (bool cq_selected, bool enable_tx, bool transmitting);

  struct FrequencyCandidate
  {
    FrequencyCandidate () = default;
    FrequencyCandidate (Frequency frequency_hz, QString band, QString mode,
                        QString region, bool default_frequency)
      : frequency_hz {frequency_hz}
      , band {std::move (band)}
      , mode {std::move (mode)}
      , region {std::move (region)}
      , default_frequency {default_frequency}
    {
    }

    Frequency frequency_hz {0};
    QString band;
    QString mode;
    QString region;
    bool default_frequency {false};
  };
  using FrequencyCandidates = QList<FrequencyCandidate>;

  struct DecodeSelection
  {
    quint64 decode_id {0};
    quint64 source_revision {0};
    QString time;
    qint32 delta_frequency {0};
    QString call;
    QString grid;
  };

  explicit JtdxWebState (QString application_name, QString application_version,
                         QString instance_id = QString {}, QObject * parent = nullptr);

  int decode_limit () const { return decode_limit_; }
  void set_decode_limit (int limit);
  quint64 revision () const { return revision_; }
  // 仅由真实 rig 观测事件递增；不能由通用 status 或 nominal 目标推导。
  quint64 rig_generation () const { return rig_generation_; }
  quint64 dx_generation () const { return dx_generation_; }

  void observe_status (Frequency target_frequency, QString const& mode,
                      QString const& dx_call, QString const& report,
                      QString const& tx_mode, bool tx_enabled, bool transmitting,
                      bool decoding, qint32 rx_df, qint32 tx_df,
                      QString const& de_call, QString const& de_grid,
                      QString const& dx_grid, bool watchdog_timeout,
                      QString const& sub_mode, bool fast_mode, bool tx_first,
                      bool force = false);
  void observe_rig (bool online, Frequency reported_frequency,
                    Frequency reported_tx_frequency, bool ptt);
  void observe_band (QString const& band);
  void observe_decode (bool is_new, QTime time, qint32 snr, float delta_time,
                       quint32 delta_frequency, QString const& mode,
                       QString const& message, bool low_confidence, bool off_air,
                       QString const& callsign = {}, QString const& grid = {});
  void observe_wspr_decode (bool is_new, QTime time, qint32 snr, float delta_time,
                            Frequency frequency, qint32 drift,
                            QString const& callsign, QString const& grid,
                            qint32 power, bool off_air);
  void observe_business_state (bool auto_sequence_enabled, QString const& qso_stage,
                               QString const& cq_state, QString const& current_tx_text);
  bool decode_selection (quint64 decode_id, DecodeSelection * selection) const;
  void observe_web_dx_selection (QString const& call, QString const& grid,
                                 QString const& source, quint64 source_decode_id,
                                 qint32 delta_frequency, QString const& time);
  void set_frequency_candidates (QString const& mode, QString const& region,
                                 FrequencyCandidates const& candidates);
  void clear_decodes ();

  // 测试时使用单调时钟，避免墙上时钟调整影响新鲜度断言。
  void set_clock_for_test (qint64 monotonic_ms);
  void advance_clock_for_test (qint64 elapsed_ms);

  QJsonObject json_snapshot () const;

private:
  struct Decode {
    quint64 id {0};
    QString time;
    qint32 snr {0};
    double delta_time {0.0};
    quint32 delta_frequency {0};
    QString mode;
    QString message;
    QString callsign;
    QString grid;
    bool low_confidence {false};
    bool off_air {false};
    bool is_new {false};
    quint64 source_revision {0};
    qint64 received_ms {0};
    bool wspr {false};
    Frequency frequency {0};
    qint32 drift {0};
    qint32 power {0};
  };

  qint64 monotonic_now () const;
  void bump_revision ();
  void trim_decodes ();
  static QJsonValue nullable_string (QString const& value);
  static QJsonValue nullable_frequency (Frequency value, bool known);
  static QJsonValue nullable_bool (bool value, bool known);

  QString application_name_;
  QString application_version_;
  QString instance_id_;
  QElapsedTimer clock_;
  bool test_clock_ {false};
  qint64 test_now_ms_ {0};
  int decode_limit_ {default_decode_limit};
  quint64 revision_ {0};
  quint64 next_decode_id_ {1};
  QList<Decode> decodes_;

  bool has_status_ {false};
  qint64 status_seen_ms_ {-1};
  QString mode_;
  QString band_;
  QString dx_call_;
  QString report_;
  QString tx_mode_;
  bool tx_enabled_ {false};
  bool transmitting_ {false};
  bool decoding_ {false};
  qint32 rx_df_ {0};
  qint32 tx_df_ {0};
  QString de_call_;
  QString de_grid_;
  QString dx_grid_;
  bool has_business_state_ {false};
  bool auto_sequence_enabled_ {false};
  QString qso_stage_;
  QString cq_state_;
  QString current_tx_text_;
  bool watchdog_timeout_ {false};
  QString sub_mode_;
  bool fast_mode_ {false};
  bool tx_first_ {false};
  Frequency target_frequency_ {0};
  bool has_target_frequency_ {false};

  QString frequency_candidate_mode_;
  QString frequency_candidate_region_;
  FrequencyCandidates frequency_candidates_;

  bool has_rig_ {false};
  bool rig_online_ {false};
  Frequency rig_frequency_ {0};
  Frequency rig_tx_frequency_ {0};
  bool rig_ptt_ {false};
  quint64 rig_generation_ {0};
  quint64 dx_generation_ {0};
  QString dx_selection_source_;
  quint64 dx_source_decode_id_ {0};
  qint32 dx_frequency_offset_ {0};
  QString dx_time_;
  qint64 rig_seen_ms_ {-1};
  qint64 decode_seen_ms_ {-1};
  QDateTime status_wall_;
  QDateTime rig_wall_;
  QDateTime decode_wall_;
};

#endif
