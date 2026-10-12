#include "Transceiver.hpp"

#include <chrono>
#include <QMetaType>
#include <QThread>

#include "moc_Transceiver.cpp"

#if !defined (QT_NO_DEBUG_STREAM)
QDebug operator << (QDebug d, Transceiver::TransceiverState const& s)
{
  d.nospace ()
    << "Transceiver::TransceiverState(online: " << (s.online_ ? "yes" : "no")
    << " Frequency {" << s.rx_frequency_ << "Hz, " << s.tx_frequency_ << "Hz} " << s.mode_
    << "; SPLIT: " << (Transceiver::TransceiverState::Split::on == s.split_ ? "on" : Transceiver::TransceiverState::Split::off == s.split_ ? "off" : "unknown")
    << "; PTT: " << (s.ptt_ ? "on" : "off")
    << "; PTT_READBACK: " << (s.ptt_readback_known_ ? "known" : "unknown")
    << "; PTT_READBACK_REQUIRED: " << (s.ptt_readback_required_ ? "yes" : "no")
    << "; PTT_READBACK_SERIAL: " << s.ptt_readback_serial_
    << "; FAST_MODE: " << (s.ft4_mode_ ? "on" : "off")
    << "; AUDIO: " << (s.audio_ ? "on" : "off")
    << "; TX_AUDIO: " << (s.tx_audio_ ? "on" : "off")
    << "; TUNE: " << (s.tune_ ? "on" : "off")
    << "; QUICK: " << (s.quick_ ? "on" : "off")
    << "; PERIOD: " << s.period_ << "sec."
    << "; BLOCKSIZE: " << s.blocksize_
    << "; SYMBOLSLENGTH: " << s.symbolslength_
    << "; FRAMESPERSYMBOL: " << s.framespersymbol_
    << "; TRFREQUENCY: " << s.trfrequency_ << "Hz"
    << "; TONESPACING: " << s.tonespacing_
    << "; SYNCHRONIZE: " << (s.synchronize_ ? "on" : "off")
    << "; DBSNR: " << s.dbsnr_
    << "; TRPERIOD: " << s.trperiod_ << "sec."
    << "; SPREAD: " << s.spread_
    << "; NSYM: " << s.nsym_
    << "; VOLUME: " << s.volume_
    << "; LEVEL: " << s.level_ << "dBm"
    << "; POWER: " << s.power_ << "mWatts"
    << "; SWR: " << s.swr_
    << ')';
  return d.space (); 
}
#endif

ENUM_QDATASTREAM_OPS_IMPL (Transceiver, MODE);

ENUM_CONVERSION_OPS_IMPL (Transceiver, MODE);

void register_transceiver_meta_types ()
{
  qRegisterMetaType<Transceiver::TransceiverState> ("Transceiver::TransceiverState");
  qRegisterMetaType<Transceiver::MODE> ("Transceiver::MODE");
  qRegisterMetaType<PttAdmissionBlock::Token> ("PttAdmissionBlock::Token");
}

void Transceiver::refresh_read_only (quint64 request_id) noexcept
{
  auto const now = std::chrono::steady_clock::now ().time_since_epoch ();
  auto const stamp = static_cast<qint64> (
    std::chrono::duration_cast<std::chrono::nanoseconds> (now).count ());
  Q_EMIT read_only_refresh_completed (
    request_id, stamp, ReadOnlyRefreshResult::unsupported,
    QStringLiteral ("unsupported"), 0, false, 0, 0);
}

void Transceiver::bind_ptt_admission (
  std::shared_ptr<PttAdmissionBlock> block) noexcept
{
  if (thread () != QThread::currentThread ()) return;
  ptt_admission_ = std::move (block);
  bind_delegated_ptt_admission (ptt_admission_);
}

void Transceiver::move_to_thread_tree (QThread * target) noexcept
{
  if (!target || thread () != QThread::currentThread ()) return;
  move_delegates_to_thread (target);
  moveToThread (target);
}

bool operator != (Transceiver::TransceiverState const& lhs, Transceiver::TransceiverState const& rhs)
{
  return lhs.online_ != rhs.online_
    || lhs.rx_frequency_ != rhs.rx_frequency_
    || lhs.tx_frequency_ != rhs.tx_frequency_
    || lhs.mode_ != rhs.mode_
    || lhs.split_ != rhs.split_
    || lhs.ptt_ != rhs.ptt_
    || lhs.ptt_readback_known_ != rhs.ptt_readback_known_
    || lhs.ptt_readback_required_ != rhs.ptt_readback_required_
    || lhs.ptt_readback_serial_ != rhs.ptt_readback_serial_
    || lhs.ft4_mode_ != rhs.ft4_mode_
    || lhs.audio_ != rhs.audio_
    || lhs.tx_audio_ != rhs.tx_audio_
    || lhs.tune_ != rhs.tune_
    || lhs.quick_ != rhs.quick_
    || lhs.period_ != rhs.period_
    || lhs.blocksize_ != rhs.blocksize_
    || lhs.symbolslength_ != rhs.symbolslength_
    || lhs.framespersymbol_ != rhs.framespersymbol_
    || lhs.trfrequency_ != rhs.trfrequency_
    || lhs.tonespacing_ != rhs.tonespacing_
    || lhs.synchronize_ != rhs.synchronize_
    || lhs.dbsnr_ != rhs.dbsnr_
    || lhs.trperiod_ != rhs.trperiod_
    || lhs.spread_ != rhs.spread_
    || lhs.nsym_ != rhs.nsym_
    || lhs.volume_ != rhs.volume_
    || lhs.level_ != rhs.level_
    || lhs.power_ != rhs.power_
    || lhs.swr_ != rhs.swr_;
}

bool operator == (Transceiver::TransceiverState const& lhs, Transceiver::TransceiverState const& rhs)
{
  return !(lhs != rhs);
}
