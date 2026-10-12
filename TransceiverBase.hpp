#ifndef TRANSCEIVER_BASE_HPP__
#define TRANSCEIVER_BASE_HPP__

#include <stdexcept>

#include <QString>
#include <QDir>
#include <QStandardPaths>
#include "Transceiver.hpp"
#include "JTDXDateTime.h"

//
// Base Transceiver Implementation
//
//  Behaviour common to all Transceiver implementations.
//
// Collaborations
//
//  Implements the Transceiver abstract  interface as template methods
//  and provides  a new abstract interface  with similar functionality
//  (do_XXXXX operations). Provides and  calls abstract interface that
//  gets  called post  the above  operations (do_post_XXXXX)  to allow
//  caching implementation etc.
//
//  A  key factor  is  to  catch all  exceptions  thrown by  sub-class
//  implementations where  the template method  is a Qt slot  which is
//  therefore  likely  to  be  called   by  Qt  which  doesn't  handle
//  exceptions. Any exceptions are converted to Transceiver::failure()
//  signals.
//
//  Sub-classes update the stored state via a protected interface.
//
// Responsibilities:
//
//  Wrap incoming  Transceiver messages catching all  exceptions in Qt
//  slot driven  messages and converting  them to Qt signals.  This is
//  done because exceptions  make concrete Transceiver implementations
//  simpler  to   write,  but  exceptions  cannot   cross  signal/slot
//  boundaries  (especially across  threads).  This  also removes  any
//  requirement for the client code to handle exceptions.
//
//  Maintain the  state of the  concrete Transceiver instance  that is
//  passed back via  the Transceiver::update(TransceiverState) signal,
//  it   is  still   the   responsibility   of  concrete   Transceiver
//  implementations to emit  the state_change signal when  they have a
//  status update.
//
//  Maintain    a   go/no-go    status   for    concrete   Transceiver
//  implementations  ensuring only  a valid  sequence of  messages are
//  passed. A concrete Transceiver instance  must be started before it
//  can receive  messages, any exception thrown  takes the Transceiver
//  offline.
//
//  Implements methods  that concrete Transceiver  implementations use
//  to update the Transceiver state.  These do not signal state change
//  to  clients  as  this  is   the  responsibility  of  the  concrete
//  Transceiver implementation, thus allowing multiple state component
//  updates to be signalled together if required.
//
class TransceiverBase
  : public Transceiver
{
  Q_OBJECT;

protected:
  TransceiverBase (QObject * parent,
                   QString failure_diagnostic_directory = QString {})
    : Transceiver {parent}
    , jtdxtime_ {nullptr}
    , last_sequence_number_ {0}
    , diagnostic_session_generation_ {0}
    , retired_ {false}
    , failure_emitted_ {false}
    , finished_emitted_ {false}
    , retired_start_logged_ {false}
    , retired_set_logged_ {false}
    , cleanup_needed_ {false}
    , failure_diagnostic_directory_ {failure_diagnostic_directory}
    , debug_file_ {QDir(QStandardPaths::writableLocation (QStandardPaths::DataLocation)).absoluteFilePath ("jtdx_debug.txt").toStdString()}
  {}

public:
  //
  // Implement the Transceiver abstract interface.
  //
  void start (unsigned sequence_number, JTDXDateTime* jtdxdatetime) noexcept override final;
  void set (TransceiverState const&,
            unsigned sequence_number,
            PttAdmissionBlock::Token ptt_admission_token) noexcept override final;
  void stop () noexcept override final;
  void refresh_read_only (quint64 request_id) noexcept override final;

  //
  // Query operations
  //
  TransceiverState const& state () const {return actual_;}
  void set_diagnostic_session_generation (quint64 generation) noexcept
  {
    diagnostic_session_generation_ = generation;
  }

protected:
  //
  // Error exception which is thrown to signal unexpected errors.
  //
  struct error
    : public std::runtime_error
  {
    explicit error (char const * const msg) : std::runtime_error (msg) {}
    explicit error (QString const& msg) : std::runtime_error (msg.toStdString ()) {}
  };

  // Template methods that sub classes implement to do what they need to do.
  //
  // These methods may throw exceptions to signal errors.
  virtual int do_start (JTDXDateTime*) = 0;  // returns resolution, See Transceiver::resolution
  virtual void do_post_start (JTDXDateTime*) {}

  virtual void do_stop () = 0;
  virtual void do_post_stop () {}

  virtual void do_frequency (Frequency, MODE, bool no_ignore) = 0;
  virtual void do_post_frequency (Frequency, MODE) {}

  virtual void do_tx_frequency (Frequency, MODE, bool no_ignore) = 0;
  virtual void do_post_tx_frequency (Frequency, MODE) {}

  virtual void do_mode (MODE) = 0;
  virtual void do_post_mode (MODE) {}

  virtual void do_ptt (bool = true) = 0;
  virtual void do_post_ptt (bool = true) {}

  virtual void do_post_ft4_mode (bool = false) {}

  virtual void do_audio (bool = false) {}
  virtual void do_tune (bool = false) {}
  virtual void do_period (double) {}
  virtual void do_blocksize (qint32) {}
  virtual void do_spread(double) {}
  virtual void do_nsym(int) {}
  virtual void do_trfrequency(double) {}
  virtual void do_txvolume (qreal) {}
  virtual void do_modulator_start(unsigned, double, double, double, bool, double, double) {}
  virtual void do_modulator_stop(bool) {}

  // Unsupported unless a concrete backend explicitly implements a bounded,
  // already-open read-only operation.  Implementations must not open or
  // mutate the rig and must report API/protocol validity separately.
  virtual ReadOnlyRefreshResult do_read_only_refresh ()
  {
    return {ReadOnlyRefreshResult::unsupported};
  }

  static qint64 worker_monotonic_ns () noexcept;
  unsigned current_control_sequence () const noexcept
  {
    return last_sequence_number_;
  }
  PttAdmissionBlock::Token current_set_admission_token () const noexcept
  {
    return current_set_admission_token_;
  }

  virtual bool do_pre_update () {return true;}

  // sub classes report rig state changes with these methods
  void update_rx_frequency (Frequency);
  void update_other_frequency (Frequency = 0);
  void update_split (bool);
  void update_mode (MODE);
  void update_PTT (bool = true);
  void update_PTT_readback_known (bool);
  void update_PTT_readback_required (bool);
  void confirm_PTT_readback (bool);
  void update_level (int = 0);
  void update_power (unsigned int = 0);
  void update_swr (unsigned int = 0);
  // Calling this eventually triggers the Transceiver::update(State) signal.
  void update_complete (bool force_signal = false);

  // sub class may asynchronously take the rig offline by calling this
  void offline (QString const& reason);

private:
  void startup ();
  void shutdown (bool force_cleanup = false);
  bool maybe_low_resolution (Frequency low_res, Frequency high_res);
  JTDXDateTime * jtdxtime_;
  // use this convenience class to notify in update methods
  class may_update
  {
  public:
    explicit may_update (TransceiverBase * self, bool force_signal = false)
      : self_ {self}
      , force_signal_ {force_signal}
    {}
    ~may_update () {self_->update_complete (force_signal_);}
  private:
    TransceiverBase * self_;
    bool force_signal_;
  };

  TransceiverState requested_;
  TransceiverState actual_;
  TransceiverState last_;
  unsigned last_sequence_number_;    // from set state operation
  quint64 diagnostic_session_generation_;
  PttAdmissionBlock::Token current_set_admission_token_ {0};
  // A Transceiver instance belongs to one rig session. Once it is stopped or
  // goes offline, queued control slots from that session must not reopen it.
  bool retired_;
  bool failure_emitted_;
  bool finished_emitted_;
  bool retired_start_logged_;
  bool retired_set_logged_;
  bool suppressed_ptt_on_logged_ {false};
  PttAdmissionBlock::Token last_suppressed_ptt_on_token_ {0};
  bool cleanup_needed_;
  QString failure_diagnostic_directory_;
  std::string debug_file_;
};

// some trace macros
#if WSJT_TRACE_CAT
#define TRACE_CAT(FAC, MSG) qDebug () << QString {"%1::%2:"}.arg ((FAC)).arg (__func__) << MSG
#else
#define TRACE_CAT(FAC, MSG)
#endif

#if WSJT_TRACE_CAT && WSJT_TRACE_CAT_POLLS
#define TRACE_CAT_POLL(FAC, MSG) qDebug () << QString {"%1::%2:"}.arg ((FAC)).arg (__func__) << MSG
#else
#define TRACE_CAT_POLL(FAC, MSG)
#endif

#endif
