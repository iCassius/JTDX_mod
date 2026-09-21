#include "JtdxLocalLog.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

#include <climits>
#include <utility>

namespace
{
  constexpr int max_area_length = 64;
  constexpr int max_message_length = 4096;

  QString sanitize (QString value, int max_length)
  {
    value.replace ('\\', QStringLiteral ("\\\\"));
    value.replace ('\r', QLatin1Char (' '));
    value.replace ('\n', QLatin1Char (' '));
    value.replace ('\t', QLatin1Char (' '));
    value.replace ('=', QStringLiteral ("%3D"));
    return value.left (max_length);
  }

  bool rotate (QString const& path, JtdxLocalLog::Limits limits)
  {
    if (limits.rotated_files < 1) return false;
    QString const rotated = path + QStringLiteral (".1");
    QFile::remove (rotated);
    if (QFile::rename (path, rotated)) return true;

    // If rotation is unavailable, truncate the active file rather than
    // allowing an unbounded diagnostic log to grow.
    QFile truncate {path};
    if (!truncate.open (QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    truncate.close ();
    return true;
  }
}

bool JtdxLocalLog::append (QDir const& directory, QString const& file_name,
                           QString area, QString message, Limits limits)
{
  if (limits.max_bytes <= 0 || file_name.isEmpty ()) return false;
  QString const path = directory.absoluteFilePath (file_name);
  QString const line = QDateTime::currentDateTimeUtc ().toString (Qt::ISODateWithMs)
      + QStringLiteral (" [") + sanitize (std::move (area), max_area_length)
      + QStringLiteral ("] ") + sanitize (std::move (message), max_message_length)
      + QLatin1Char ('\n');
  QByteArray bytes = line.toUtf8 ();
  if (bytes.size () > limits.max_bytes)
    bytes = bytes.left (static_cast<int> (qMin<qint64> (limits.max_bytes, INT_MAX)));
  if (bytes.isEmpty ()) return false;

  QFileInfo const info {path};
  if (info.exists () && info.size () + bytes.size () > limits.max_bytes
      && !rotate (path, limits)) return false;

  QFile log {path};
  if (!log.open (QIODevice::WriteOnly | QIODevice::Append)) return false;
  qint64 const written = log.write (bytes);
  log.close ();
  return written == bytes.size ();
}
