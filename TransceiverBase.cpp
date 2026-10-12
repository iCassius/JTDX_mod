#include "TransceiverBase.hpp"

#include <exception>
#include <chrono>

#include <QDateTime>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QDebug>

#include "JtdxLocalLog.hpp"
#include "moc_TransceiverBase.cpp"

namespace
{
  auto const unexpected = TransceiverBase::tr ("Unexpected rig error");

  qint64 stop_diagnostic_monotonic_ns () noexcept
  {
    auto const now = std::chrono::steady_clock::now ().time_since_epoch ();
    return static_cast<qint64> (
      std::chrono::duration_cast<std::chrono::nanoseconds> (now).count ());
  }

  void append_failure_diagnostic (QDir const& directory, char const * stage,
                                  char const * failure_signal) noexcept
    {
      try
        {
          JtdxLocalLog::append (
            directory,
            QStringLiteral ("jtdx_recovery.log"), QStringLiteral ("rig-control"),
            QStringLiteral ("transceiver_offline stage=%1 failure_signal=%2 reason_present=true")
              .arg (QString::fromLatin1 (stage),
                    QString::fromLatin1 (failure_signal)));
        }
      catch (...)
        {
          // A diagnostic write must not escape into transceiver control flow.
        }
    }

  void append_retired_command_diagnostic (QDir const& directory,
                                          char const * action,
                                          unsigned sequence,
                                          unsigned last_sequence) noexcept
  {
    try
      {
        JtdxLocalLog::append (
          directory,
          QStringLiteral ("jtdx_recovery.log"), QStringLiteral ("rig-control"),
          QStringLiteral ("retired_command_ignored action=%1 sequence=%2 last_sequence=%3 retired=true")
            .arg (QString::fromLatin1 (action))
            .arg (sequence).arg (last_sequence));
      }
    catch (...)
      {
        // A diagnostic write must not escape into transceiver control flow.
      }
  }

  void append_ptt_admission_rejected (QDir const& directory,
                                      unsigned sequence,
                                      PttAdmissionBlock::Token queued_token,
                                      PttAdmissionBlock::Token current_token,
                                      char const * stage) noexcept
  {
    try
      {
        JtdxLocalLog::append (
          directory, QStringLiteral ("jtdx_recovery.log"),
          QStringLiteral ("rig-control"),
          QStringLiteral ("ptt_on_suppressed sequence=%1 queued_token=%2 current_token=%3 stage=%4")
            .arg (sequence).arg (queued_token).arg (current_token)
            .arg (QString::fromLatin1 (stage)));
      }
    catch (...)
      {
        // Diagnostics must not change control flow.
      }
  }

  void append_ptt_off_diagnostic (QString const& directory_path,
                                  char const * event, quint64 generation,
                                  unsigned sequence,
                                  PttAdmissionBlock::Token admission_token,
                                  char const * outcome, char const * exception_type,
                                  bool requested_online, bool cleanup_needed,
                                  char const * reason) noexcept
  {
    try
      {
        QDir const directory {directory_path.isEmpty ()
          ? QStandardPaths::writableLocation (QStandardPaths::DataLocation)
          : directory_path};
        auto const local_now = QDateTime::currentDateTime ();
        auto const utc_now = local_now.toUTC ();
        JtdxLocalLog::append (
          directory, QStringLiteral ("jtdx_recovery.log"),
          QStringLiteral ("rig-control"),
          QStringLiteral ("stop_diag event=%1 utc=%2 local=%3 worker_monotonic_ns=%4 "
                          "session_generation=%5 control_sequence=%6 admission_token=%7 "
                          "outcome=%8 exception_type=%9 requested_online=%10 "
                          "cleanup_needed=%11 reason=%12 result_scope=driver_method_only "
                          "readback=unknown")
            .arg (QString::fromLatin1 (event ? event : "unknown"))
            .arg (utc_now.toString (Qt::ISODateWithMs))
            .arg (local_now.toString (Qt::ISODateWithMs))
            .arg (stop_diagnostic_monotonic_ns ())
            .arg (generation).arg (sequence).arg (admission_token)
            .arg (QString::fromLatin1 (outcome ? outcome : "unknown"))
            .arg (QString::fromLatin1 (exception_type ? exception_type : "unknown"))
            .arg (requested_online ? 1 : 0).arg (cleanup_needed ? 1 : 0)
            .arg (QString::fromLatin1 (reason ? reason : "none")));
      }
    catch (...)
      {
        // Stop diagnostics must not change driver or worker control flow.
      }
  }
}

qint64 TransceiverBase::worker_monotonic_ns () noexcept
{
  return stop_diagnostic_monotonic_ns ();
}

void TransceiverBase::start (unsigned sequence_number,JTDXDateTime * jtdxdatetime) noexcept
{
  if (retired_)
    {
      if (!retired_start_logged_)
        {
          retired_start_logged_ = true;
          QDir const diagnostic_directory {failure_diagnostic_directory_.isEmpty ()
            ? QStandardPaths::writableLocation (QStandardPaths::DataLocation)
            : failure_diagnostic_directory_};
          append_retired_command_diagnostic (diagnostic_directory, "start",
                                             sequence_number, last_sequence_number_);
          TRACE_CAT ("TransceiverBase", "retired command ignored action=start #:" << sequence_number);
        }
      return;
    }

  QString message;
#if JTDX_DEBUG_TO_FILE
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver start #:%d\n",sequence_number);
  else
    fprintf(pFile,"%s Transceiver start #:%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
  fclose (pFile);
#endif
  try
    {
      last_sequence_number_ = sequence_number;
      jtdxtime_ = jtdxdatetime;
      may_update u {this, true};
      shutdown (true);
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"Transceiver start shutdown done\n");
      else
        fprintf(pFile,"%s Transceiver start shutdown done\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
      fclose (pFile);
#endif
      startup ();
    }
  catch (std::exception const& e)
    {
      message = e.what ();
    }
  catch (...)
    {
      message = unexpected;
    }
  if (!message.isEmpty ())
    {
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"Transceiver start message %s\n",message.toStdString().c_str());
      else
        fprintf(pFile,"%s Transceiver start message %s\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),message.toStdString().c_str());
      fclose (pFile);
#endif
      offline (message);
    }
#if JTDX_DEBUG_TO_FILE
  pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver start done\n");
  else
    fprintf(pFile,"%s Transceiver start done\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
  fclose (pFile);
#endif
}

void TransceiverBase::set (TransceiverState const& s,
                           unsigned sequence_number,
                           PttAdmissionBlock::Token ptt_admission_token) noexcept
{
  if (retired_)
    {
      if (!retired_set_logged_)
        {
          retired_set_logged_ = true;
          QDir const diagnostic_directory {failure_diagnostic_directory_.isEmpty ()
            ? QStandardPaths::writableLocation (QStandardPaths::DataLocation)
            : failure_diagnostic_directory_};
          append_retired_command_diagnostic (diagnostic_directory, "set",
                                             sequence_number, last_sequence_number_);
          TRACE_CAT ("TransceiverBase", "retired command ignored action=set #:" << sequence_number);
        }
      return;
    }

  TRACE_CAT ("TransceiverBase", "#:" << sequence_number << s);
  bool ptt_off_attempted {false};
  char const * ptt_off_outcome {"not_attempted"};
  char const * ptt_off_exception_type {"none"};
#if JTDX_DEBUG_TO_FILE
  long long ms = 0;
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver set state %d #:%d\n",s.online(),sequence_number);
  else {
    fprintf(pFile,"%s Transceiver set state %d #:%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),s.online(),sequence_number);
    ms = jtdxtime_->currentMSecsSinceEpoch2();
  }
  fclose (pFile);
#endif
  QString message;
  try
    {
      last_sequence_number_ = sequence_number;
      current_set_admission_token_ = ptt_admission_token;
      may_update u {this, true};
      QDir const diagnostic_directory {failure_diagnostic_directory_.isEmpty ()
        ? QStandardPaths::writableLocation (QStandardPaths::DataLocation)
        : failure_diagnostic_directory_};
      auto log_suppressed_ptt_on = [this, &diagnostic_directory,
                                    sequence_number, ptt_admission_token]
        (char const * stage) {
          if (suppressed_ptt_on_logged_
              && last_suppressed_ptt_on_token_ == ptt_admission_token) return;
          suppressed_ptt_on_logged_ = true;
          last_suppressed_ptt_on_token_ = ptt_admission_token;
          append_ptt_admission_rejected (
            diagnostic_directory, sequence_number, ptt_admission_token,
            current_ptt_admission_token (), stage);
        };
      bool was_online {requested_.online ()};
      if (!s.online ()) retired_ = true;
      if (!s.online () && was_online)
        {
#if JTDX_DEBUG_TO_FILE
          pFile = fopen (debug_file_.c_str(),"a");
          if (jtdxtime_ == nullptr)
            fprintf(pFile,"Transceiver shutdown #:%d ",sequence_number);
          else
            fprintf(pFile,"%s Transceiver shutdown #:%d ",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
          fclose (pFile);
#endif
          shutdown ();
        }
      else if (s.online () && !was_online)
        {
#if JTDX_DEBUG_TO_FILE
          pFile = fopen (debug_file_.c_str(),"a");
          if (jtdxtime_ == nullptr)
            fprintf(pFile," restart shutdown #:%d ",sequence_number);
          else
            fprintf(pFile,"%s restart shutdown #:%d ",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
          fclose (pFile);
#endif
          shutdown (true);
#if JTDX_DEBUG_TO_FILE
          pFile = fopen (debug_file_.c_str(),"a");
          if (jtdxtime_ == nullptr)
            fprintf(pFile," start #:%d",sequence_number);
          else
            fprintf(pFile,"%s start #:%d",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
          fclose (pFile);
#endif
          startup ();
        }
      if (requested_.online ())
        {
          bool audio_cmd {false};
          if (requested_.blocksize() != s.blocksize()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d blocksize=%d\n",sequence_number,s.blocksize());
            else
              fprintf(pFile,"%s Timing #:%d blocksize=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.blocksize());
            fclose (pFile);
#endif
            do_blocksize (s.blocksize());
            audio_cmd = true;
            requested_.blocksize (s.blocksize ());
          }
          if (requested_.period() != s.period()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d period=%0.1f\n",sequence_number,s.period());
            else
              fprintf(pFile,"%s Timing #:%d period=%0.1f\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.period());
            fclose (pFile);
#endif
            do_period (s.period());
            audio_cmd = true;
            requested_.period (s.period ());
          }
          if (requested_.spread() != s.spread()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d spread=%0.1f\n",sequence_number,s.spread());
            else
              fprintf(pFile,"%s Timing #:%d spread=%0.1f\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.spread());
            fclose (pFile);
#endif
            do_spread (s.spread());
            audio_cmd = true;
            requested_.spread (s.spread ());
          }
          if (requested_.nsym() != s.nsym()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d nsym=%d\n",sequence_number,s.nsym());
            else
              fprintf(pFile,"%s Timing #:%d nsym=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.nsym());
            fclose (pFile);
#endif
            do_nsym (s.nsym());
            audio_cmd = true;
            requested_.nsym (s.nsym ());
          }
          if (requested_.trfrequency() != s.trfrequency()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d trfrequency=%0.1f\n",sequence_number,s.trfrequency());
            else
              fprintf(pFile,"%s Timing #:%d trfrequency=%0.1f\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.trfrequency());
            fclose (pFile);
#endif
            do_trfrequency (s.trfrequency());
            audio_cmd = true;
            requested_.trfrequency (s.trfrequency ());
          }
          if (requested_.volume() != s.volume()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d volume=%0.1f\n",sequence_number,s.volume());
            else
              fprintf(pFile,"%s Timing #:%d volume=%0.1f\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.volume());
            fclose (pFile);
#endif
            do_txvolume (s.volume());
            audio_cmd = true;
            requested_.volume (s.volume ());
          }
          if (requested_.audio() != s.audio()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d audio=%d\n",sequence_number,s.audio());
            else
              fprintf(pFile,"%s Timing #:%d audio=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.audio());
            fclose (pFile);
#endif
            do_audio (s.audio());
            audio_cmd = true;
            requested_.audio (s.audio ());
          }
          if (requested_.tx_audio() != s.tx_audio()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d tx_audio=%d\n",sequence_number,s.tx_audio());
            else
              fprintf(pFile,"%s Timing #:%d tx_audio=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.tx_audio());
            fclose (pFile);
#endif
            if (s.tx_audio()) {
              do_modulator_start(s.symbolslength (), s.framespersymbol (), s.trfrequency (), s.tonespacing (), s.synchronize (),s.dbsnr (), s.trperiod ());
              requested_.symbolslength(s.symbolslength ());
              requested_.framespersymbol(s.framespersymbol ());
              requested_.trfrequency(s.trfrequency ());
              requested_.tonespacing(s.tonespacing ());
              requested_.synchronize(s.synchronize ());
              requested_.dbsnr(s.dbsnr ());
              requested_.trperiod(s.trperiod ());
            } else {
              do_modulator_stop(s.quick ());
              requested_.quick (s.quick ());
            }
            audio_cmd = true;

            requested_.tx_audio (s.tx_audio ());
          }


          if (requested_.tune() != s.tune()) {
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"#:%d tune=%d\n",sequence_number,s.tune());
            else
              fprintf(pFile,"%s Timing #:%d tune=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.tune());
            fclose (pFile);
#endif
            do_tune (s.tune());
            audio_cmd = true;
            requested_.tune (s.tune ());
          }
          if (!audio_cmd) {
            bool ptt_on {false};
            bool ptt_off {false};
            if (requested_.ft4_mode() != s.ft4_mode()) {
  #if JTDX_DEBUG_TO_FILE
              pFile = fopen (debug_file_.c_str(),"a");
              if (jtdxtime_ == nullptr)
                fprintf(pFile,"#:%d ft4_mode=%d\n",sequence_number,s.ft4_mode());
              else
                fprintf(pFile,"%s Timing #:%d ft4_mode=%d\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.ft4_mode());
              fclose (pFile);
  #endif
              do_post_ft4_mode (s.ft4_mode());
              requested_.ft4_mode (s.ft4_mode ());
            }
            if (s.ptt () != requested_.ptt ())
              {
                // Reject revoked queued ON commands before their state can
                // influence backend about-to-TX frequency handling.
                auto const admitted = ptt_start_admitted (ptt_admission_token);
                ptt_on = s.ptt () && admitted;
                if (s.ptt () && !admitted)
                  log_suppressed_ptt_on ("revoked_before_tx_frequency");
                ptt_off = !s.ptt ();
              }
            if (ptt_off)
              {
                ptt_off_attempted = true;
                ptt_off_outcome = "pending";
  #if JTDX_DEBUG_TO_FILE
                pFile = fopen (debug_file_.c_str(),"a");
                if (jtdxtime_ == nullptr)
                  fprintf(pFile,"#:%d ptt_off\n",sequence_number);
                else
                  fprintf(pFile,"%s Timing #:%d ptt_off\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
                fclose (pFile);
  #endif
                try
                  {
                    do_ptt (false);
                  }
                catch (std::exception const&)
                  {
                    ptt_off_outcome = "exception";
                    ptt_off_exception_type = "std_exception";
                    throw;
                  }
                catch (...)
                  {
                    ptt_off_outcome = "exception";
                    ptt_off_exception_type = "unknown";
                    throw;
                  }
                ptt_off_outcome = "returned";
                do_post_ptt (false);
                if (!requested_.audio()) QThread::msleep (100); // some rigs cannot process CAT
                                       // commands while switching from
                                       // Tx to Rx
              }
            if (s.frequency ()    // ignore bogus zero frequencies
                && ((s.frequency () != requested_.frequency () // and QSY
                     || (s.mode () != UNK && s.mode () != requested_.mode ())) // or mode change
                    || ptt_off))       // or just returned to rx
              {
  #if JTDX_DEBUG_TO_FILE
                pFile = fopen (debug_file_.c_str(),"a");
                if (jtdxtime_ == nullptr)
                  fprintf(pFile,"#:%d do_frequency %lld\n",sequence_number,s.frequency ());
                else
                  fprintf(pFile,"%s Timing #:%d do_frequency %lld\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.frequency ());
                fclose (pFile);
  #endif
                requested_.frequency (s.frequency ());
                requested_.mode (s.mode ());
                do_frequency (s.frequency (), s.mode (), ptt_off);
                do_post_frequency (s.frequency (), s.mode ());

                // record what actually changed
                requested_.frequency (actual_.frequency ());
                requested_.mode (actual_.mode ());
              }
            else if (!s.tx_frequency ()
                || (s.tx_frequency () > 10000 // ignore bogus startup values
                    && s.tx_frequency () < std::numeric_limits<Frequency>::max () - 10000))
              {
                if ((s.tx_frequency () != requested_.tx_frequency () // and QSY
                     || (s.mode () != UNK && s.mode () != requested_.mode ())) // or mode change
                    // || s.split () != requested_.split ())) // or split change
                    || (s.tx_frequency () && ptt_on)) // or about to tx split
                  {
                    if (ptt_on && !ptt_start_admitted (ptt_admission_token))
                      {
                        ptt_on = false;
                        log_suppressed_ptt_on ("revoked_before_tx_frequency");
                      }
#if JTDX_DEBUG_TO_FILE
                    pFile = fopen (debug_file_.c_str(),"a");
                    if (jtdxtime_ == nullptr)
                      fprintf(pFile,"#:%d do_tx_frequency %lld\n",sequence_number,s.tx_frequency ());
                    else
                      fprintf(pFile,"%s Timing #:%d do_tx_frequency %lld\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,s.tx_frequency ());
                    fclose (pFile);
  #endif
                    requested_.tx_frequency (s.tx_frequency ());
                    requested_.split (s.tx_frequency () != 0);
                    do_tx_frequency (s.tx_frequency (), s.mode (), ptt_on);
                    do_post_tx_frequency (s.tx_frequency (), s.mode ());

                    // record what actually changed
                    requested_.tx_frequency (actual_.tx_frequency ());
                    requested_.split (actual_.split ());
                  }
              }
            if (ptt_on && !ptt_start_admitted (ptt_admission_token))
              {
                ptt_on = false;
                log_suppressed_ptt_on ("revoked_before_ptt");
              }
            if (ptt_on)
              {
  #if JTDX_DEBUG_TO_FILE
                pFile = fopen (debug_file_.c_str(),"a");
                if (jtdxtime_ == nullptr)
                  fprintf(pFile,"#:%d ptt_on\n",sequence_number);
                else
                  fprintf(pFile,"%s Timing #:%d ptt_on\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
                fclose (pFile);
  #endif
                do_ptt (true);
                // Cancellation cannot retract an entered driver call, but it
                // must prevent a post hook from re-enabling TX.
                if (ptt_start_admitted (ptt_admission_token))
                  do_post_ptt (true);
  //              QThread::msleep (100 + ms); // some rigs cannot process CAT
                                       // commands while switching from
                                       // Rx to Tx
              }

            // record what actually changed
            if (ptt_on || ptt_off) requested_.ptt (actual_.ptt ());
          }
        } else {
#if JTDX_DEBUG_TO_FILE
          pFile = fopen (debug_file_.c_str(),"a");
          if (jtdxtime_ == nullptr)
            fprintf (pFile,"#:%d NOT ONLINE\n",sequence_number);
          else
            fprintf (pFile,"%s #:%d NOT ONLINE\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number);
          fclose (pFile);
#endif
        }
    }
  catch (std::exception const& e)
    {
      message = e.what ();
    }
  catch (...)
    {
      message = unexpected;
    }
  
  if (!message.isEmpty ())
    {
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"#:%d set message %s\n",sequence_number,message.toStdString().c_str());
      else
        fprintf(pFile,"%s #:%d set message %s\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),sequence_number,message.toStdString().c_str());
      fclose (pFile);
#endif
      offline (message);
    }
  if (ptt_off_attempted)
    append_ptt_off_diagnostic (
      failure_diagnostic_directory_, "ptt_off_driver",
      diagnostic_session_generation_, sequence_number,
      current_set_admission_token_, ptt_off_outcome, ptt_off_exception_type,
      requested_.online (), cleanup_needed_, "runtime_set");
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"Transceiver set %d #:%d end\n",s.online(),sequence_number);
      else
        fprintf(pFile,"%s Transceiver set %d #:%d end %lld ms.\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),s.online(),sequence_number,jtdxtime_->currentMSecsSinceEpoch2() - ms);
      fclose (pFile);
#endif
}

void TransceiverBase::startup ()
{
  QString message;
#if JTDX_DEBUG_TO_FILE
  long long ms = 0;
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver startup\n");
  else {
    fprintf(pFile,"%s Transceiver startup\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
    ms = jtdxtime_->currentMSecsSinceEpoch2();
  }
  fclose (pFile);
#endif
  try
    {
      // Mark cleanup as required before entering driver code: do_start may
      // partially open a device and then throw.
      cleanup_needed_ = true;
      actual_.online (true);
      requested_.online (true);
      auto res = do_start (jtdxtime_);
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"do_start\n");
      else
        fprintf(pFile,"%s Timing do_start\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
      fclose (pFile);
#endif
      do_post_start (jtdxtime_);
      Q_EMIT resolution (res);
    }
  catch (std::exception const& e)
    {
      message = e.what ();
    }
  catch (...)
    {
      message = unexpected;
    }
  if (!message.isEmpty ())
    {
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"Transceiver startup message %s\n",message.toStdString().c_str());
      else
        fprintf(pFile,"%s Transceiver startup message %s\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),message.toStdString().c_str());
      fclose (pFile);
#endif
      offline (message);
    }
#if JTDX_DEBUG_TO_FILE
      pFile = fopen (debug_file_.c_str(),"a");
      if (jtdxtime_ == nullptr)
        fprintf(pFile,"Transceiver startup end\n");
      else
        fprintf(pFile,"%s Transceiver startup end %lld ms.\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),jtdxtime_->currentMSecsSinceEpoch2() - ms);
      fclose (pFile);
#endif
}

void TransceiverBase::shutdown (bool force_cleanup)
{
#if JTDX_DEBUG_TO_FILE
  long long ms = 0;
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr) {
    fprintf(pFile,"Transceiver shutdown\n");
  }
  else {
    fprintf(pFile,"%s Transceiver shutdown\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
    ms = jtdxtime_->currentMSecsSinceEpoch2();
  }
  fclose (pFile);
#endif
  may_update u {this};
  if (!cleanup_needed_ && !force_cleanup) return;
  if (force_cleanup) cleanup_needed_ = true;
  bool const shutdown_requested_online = requested_.online ();
  bool const shutdown_cleanup_needed = cleanup_needed_;
  char const * shutdown_ptt_outcome = shutdown_requested_online ? "pending" : "skipped";
  char const * shutdown_ptt_exception_type = "none";
#if JTDX_DEBUG_TO_FILE
  pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"may_update\n");
  else
    fprintf(pFile,"%s Timing may_update\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
  fclose (pFile);
#endif
  if (requested_.online ())
    {
      try
        {
          // try and ensure PTT isn't left set
          try
            {
              do_ptt (false);
            }
          catch (std::exception const&)
            {
              shutdown_ptt_outcome = "exception";
              shutdown_ptt_exception_type = "std_exception";
              throw;
            }
          catch (...)
            {
              shutdown_ptt_outcome = "exception";
              shutdown_ptt_exception_type = "unknown";
              throw;
            }
          shutdown_ptt_outcome = "returned";
#if JTDX_DEBUG_TO_FILE
          pFile = fopen (debug_file_.c_str(),"a");
          if (jtdxtime_ == nullptr)
            fprintf(pFile,"do_ptt:0\n");
          else
            fprintf(pFile,"%s Timing do_ptt:0\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
          fclose (pFile);
#endif
          do_post_ptt (false);
          if (requested_.split ())
            {
              // try and reset split mode
              do_tx_frequency (0, UNK, true);
#if JTDX_DEBUG_TO_FILE
            pFile = fopen (debug_file_.c_str(),"a");
            if (jtdxtime_ == nullptr)
              fprintf(pFile,"do_tx_freq:0\n");
            else
              fprintf(pFile,"%s Timing do_tx_freq:0\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
            fclose (pFile);
#endif
              do_post_tx_frequency (0, UNK);
            }
        }
      catch (...)
        {
          // don't care about exceptions
        }
    }
//  printf("Transceiver shutdow b4 dostop()\n");
  try
    {
      do_stop ();
    }
  catch (std::exception const&)
    {
      append_ptt_off_diagnostic (
        failure_diagnostic_directory_, "shutdown_ptt_off",
        diagnostic_session_generation_, last_sequence_number_,
        current_set_admission_token_, shutdown_ptt_outcome,
        shutdown_ptt_exception_type, shutdown_requested_online,
        shutdown_cleanup_needed,
        shutdown_requested_online ? "driver_call" : "rig_not_requested_online");
      append_ptt_off_diagnostic (
        failure_diagnostic_directory_, "shutdown_backend_stop",
        diagnostic_session_generation_, last_sequence_number_,
        current_set_admission_token_, "exception", "std_exception",
        requested_.online (), cleanup_needed_, "backend_stop");
      throw;
    }
  catch (...)
    {
      append_ptt_off_diagnostic (
        failure_diagnostic_directory_, "shutdown_ptt_off",
        diagnostic_session_generation_, last_sequence_number_,
        current_set_admission_token_, shutdown_ptt_outcome,
        shutdown_ptt_exception_type, shutdown_requested_online,
        shutdown_cleanup_needed,
        shutdown_requested_online ? "driver_call" : "rig_not_requested_online");
      append_ptt_off_diagnostic (
        failure_diagnostic_directory_, "shutdown_backend_stop",
        diagnostic_session_generation_, last_sequence_number_,
        current_set_admission_token_, "exception", "unknown",
        requested_.online (), cleanup_needed_, "backend_stop");
      throw;
    }
#if JTDX_DEBUG_TO_FILE
  pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"do_stop\n");
  else
    fprintf(pFile,"%s Timing do_stop\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
  fclose (pFile);
#endif
  do_post_stop ();
  cleanup_needed_ = false;
  actual_ = TransceiverState {};
  requested_ = TransceiverState {};
#if JTDX_DEBUG_TO_FILE
  pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver shutdown end\n");
  else
    fprintf(pFile,"%s Transceiver shutdown end %lld ms.\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),jtdxtime_->currentMSecsSinceEpoch2() - ms);
      fclose (pFile);
#endif
//  jtdxtime_ = nullptr;
  append_ptt_off_diagnostic (
    failure_diagnostic_directory_, "shutdown_ptt_off",
    diagnostic_session_generation_, last_sequence_number_,
    current_set_admission_token_, shutdown_ptt_outcome,
    shutdown_ptt_exception_type, shutdown_requested_online,
    shutdown_cleanup_needed,
    shutdown_requested_online ? "driver_call" : "rig_not_requested_online");
  append_ptt_off_diagnostic (
    failure_diagnostic_directory_, "shutdown_backend_stop",
    diagnostic_session_generation_, last_sequence_number_,
    current_set_admission_token_, "returned", "none",
    shutdown_requested_online, shutdown_cleanup_needed, "backend_stop_and_post_stop_returned");
}

void TransceiverBase::stop () noexcept
{
  if (finished_emitted_) return;

  retired_ = true;
  QString message;
#if JTDX_DEBUG_TO_FILE
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver stop\n");
  else
    fprintf(pFile,"%s Transceiver stop\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
  fclose (pFile);
#endif
  try
    {
      shutdown ();
    }
  catch (std::exception const& e)
    {
      message = e.what ();
    }
  catch (...)
    {
      message = unexpected;
    }
  if (!message.isEmpty ()) offline (message);
  if (!finished_emitted_)
    {
      // A stop is terminal even when cleanup failed twice: give the owner a
      // chance to delete this retired worker after best-effort cleanup.
      finished_emitted_ = true;
      Q_EMIT finished ();
    }
#if JTDX_DEBUG_TO_FILE
  pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver stop end\n");
  else
    fprintf(pFile,"%s Transceiver stop end\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str());
  fclose (pFile);
#endif
}

void TransceiverBase::update_rx_frequency (Frequency rx)
{
  if (rx)
    {
      actual_.frequency (rx);
      requested_.frequency (rx);    // track rig changes
    }
}

void TransceiverBase::update_other_frequency (Frequency tx)
{
  actual_.tx_frequency (tx);
}

void TransceiverBase::update_split (bool state)
{
  actual_.split (state);
}

void TransceiverBase::update_mode (MODE m)
{
  actual_.mode (m);
  requested_.mode (m);    // track rig changes
}

void TransceiverBase::update_PTT (bool state)
{
  actual_.ptt (state);
}

void TransceiverBase::refresh_read_only (quint64 request_id) noexcept
{
  ReadOnlyRefreshResult result;
  if (retired_ || !requested_.online () || !actual_.online ())
    {
      result.status = ReadOnlyRefreshResult::inactive;
    }
  else
    {
      try
        {
          result = do_read_only_refresh ();
        }
      catch (...)
        {
          result.status = ReadOnlyRefreshResult::internal_failure;
          result.state_changed = state ().ptt_readback_required ()
            && state ().ptt_readback_known ();
          if (state ().ptt_readback_required ())
            update_PTT_readback_known (false);
        }
    }

  if (result.state_changed)
    {
      try
        {
          // Use the ordinary update gate and preserve the sequence of the
          // last control command.  A diagnostic refresh is not a command.
          update_complete ();
        }
      catch (...)
        {
          // A read-only probe must not escape through a Qt slot.
        }
    }

  Q_EMIT read_only_refresh_completed (
    request_id, worker_monotonic_ns (), static_cast<int> (result.status),
    result.operation, result.api_return, result.protocol_valid,
    current_control_sequence (), state ().ptt_readback_serial ());
}

void TransceiverBase::update_PTT_readback_known (bool known)
{
  actual_.ptt_readback_known (known);
}

void TransceiverBase::update_PTT_readback_required (bool required)
{
  actual_.ptt_readback_required (required);
}

void TransceiverBase::confirm_PTT_readback (bool state)
{
  auto serial = actual_.ptt_readback_serial () + 1;
  if (!serial) ++serial;
  actual_.ptt_readback_serial (serial);
  actual_.ptt_readback_known (true);
  actual_.ptt (state);
}

void TransceiverBase::update_level (int l)
{
  actual_.level (l);
//  requested_.level (l);    // track rig changes
}

void TransceiverBase::update_power (unsigned int p)
{
  actual_.power (p);
//  requested_.power (p);    // track rig changes
}

void TransceiverBase::update_swr (unsigned int p)
{
  actual_.swr (p);
//  requested_.swr (p);    // track rig changes
}

void TransceiverBase::update_complete (bool force_signal)
{
  if ((do_pre_update () && actual_ != last_) || force_signal)
    {
      Q_EMIT update (actual_, last_sequence_number_);
      last_ = actual_;
    }
}

void TransceiverBase::offline (QString const& reason)
{
  retired_ = true;
  QDir const diagnostic_directory {failure_diagnostic_directory_.isEmpty ()
    ? QStandardPaths::writableLocation (QStandardPaths::DataLocation)
    : failure_diagnostic_directory_};
  if (finished_emitted_)
    {
      append_failure_diagnostic (diagnostic_directory, "finished",
                                 failure_emitted_ ? "already_emitted"
                                                  : "suppressed_finished");
      return;
    }
  bool const emitFailureThisCall = !failure_emitted_;
  append_failure_diagnostic (diagnostic_directory, "entered",
                             emitFailureThisCall ? "will_emit_this_call"
                                                 : "already_emitted");
#if JTDX_DEBUG_TO_FILE
  FILE * pFile = fopen (debug_file_.c_str(),"a");
  if (jtdxtime_ == nullptr)
    fprintf(pFile,"Transceiver offline %s\n",reason.toStdString().c_str());
  else
    fprintf(pFile,"%s Transceiver offline %s\n",jtdxtime_->currentDateTimeUtc2().toString("hh:mm:ss.zzz").toStdString().c_str(),reason.toStdString().c_str());
  fclose (pFile);
#endif
  if (emitFailureThisCall)
    {
      failure_emitted_ = true;
      Q_EMIT failure (reason);
    }
  append_failure_diagnostic (diagnostic_directory, "after_emit",
                             emitFailureThisCall ? "emitted_this_call"
                                                 : "already_emitted");
  try
    {
      shutdown ();
    }
  catch (...)
    {
      // don't care
    }
}
