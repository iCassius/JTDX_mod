#ifndef AUTOSEQ_DIAGNOSTIC_FORMATTER_HPP__
#define AUTOSEQ_DIAGNOSTIC_FORMATTER_HPP__

#include <QUrl>
#include <QString>
#include <QStringList>

namespace AutoSeqDiagnosticFormatter
{
  inline QString encoded_field (QString const& key, QString const& value)
  {
    return key + QLatin1Char ('=')
      + QString::fromLatin1 (QUrl::toPercentEncoding (value));
  }

  inline void append_field (QStringList& fields, QString const& key,
                            QString const& value)
  {
    fields.append (encoded_field (key, value));
  }

  inline QString line (QStringList const& fields)
  {
    return fields.join (QLatin1Char (' '));
  }
}

#endif
