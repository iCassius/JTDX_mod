#include "JtdxLocalLog.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <climits>
#include <utility>

namespace
{
  constexpr int max_area_length = 64;
  constexpr int max_message_length = 4096;
  QMutex log_mutex;

  QString sanitize (QString value, int max_length)
  {
    value.replace ('\\', QStringLiteral ("\\\\"));
    value.replace ('\r', QLatin1Char (' '));
    value.replace ('\n', QLatin1Char (' '));
    value.replace ('\t', QLatin1Char (' '));
    return value.left (max_length);
  }

  bool limit_file (QString const& path, qint64 max_bytes)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly)) return false;
    qint64 const size = file.size ();
    if (size <= max_bytes) return true;
    if (max_bytes > INT_MAX || !file.seek (size - max_bytes)) return false;
    QByteArray tail = file.read (max_bytes);
    file.close ();
    if (tail.size () != max_bytes) return false;
    QFile rewrite {path};
    if (!rewrite.open (QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    qint64 const written = rewrite.write (tail);
    rewrite.close ();
    return written == tail.size ();
  }

  bool rotate (QString const& path, JtdxLocalLog::Limits limits)
  {
    if (limits.rotated_files < 1 || limits.rotated_files > 16) return false;
    for (int index = limits.rotated_files; index > 0; --index)
      {
        QString const source = index == 1
          ? path : path + QLatin1Char ('.') + QString::number (index - 1);
        QString const destination = path + QLatin1Char ('.') + QString::number (index);
        QFile::remove (destination);
        QFileInfo const source_info {source};
        if (!source_info.exists ()) continue;
        if (!source_info.isFile ())
          {
            // A non-file at an archive path must not be moved or treated as a
            // readable log. Fall back to a bounded active file instead.
            QFile truncate {path};
            if (!truncate.open (QIODevice::WriteOnly | QIODevice::Truncate)) return false;
            truncate.close ();
            return true;
          }
        if (!QFile::rename (source, destination))
          {
            // Keep append bounded even if the requested archive shift fails.
            QFile truncate {path};
            if (!truncate.open (QIODevice::WriteOnly | QIODevice::Truncate)) return false;
            truncate.close ();
            return true;
          }
        if (limit_file (destination, limits.max_bytes)) continue;
        QFile truncate_rotated {destination};
        if (truncate_rotated.open (QIODevice::WriteOnly | QIODevice::Truncate))
          truncate_rotated.close ();
        return false;
      }
    return true;
  }
}

bool JtdxLocalLog::append (QDir const& directory, QString const& file_name,
                           QString area, QString message, Limits limits)
{
  try
    {
      if (limits.max_bytes <= 0 || file_name.isEmpty ()) return false;
      QMutexLocker locker {&log_mutex};
      QString const path = directory.absoluteFilePath (file_name);
      QString const line = QDateTime::currentDateTime ().toString (Qt::ISODateWithMs)
          + QStringLiteral (" [") + sanitize (std::move (area), max_area_length)
          + QStringLiteral ("] ") + sanitize (std::move (message), max_message_length)
          + QLatin1Char ('\n');
      QByteArray bytes = line.toUtf8 ();
      if (bytes.size () > limits.max_bytes)
        {
          int const limit = static_cast<int> (qMin<qint64> (limits.max_bytes, INT_MAX));
          bytes.truncate (qMax (0, limit - 1));
          bytes.append ('\n');
        }
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
  catch (...)
    {
      // Diagnostics must never escape into CAT, AutoSeq, or TX control paths.
      return false;
    }
}
