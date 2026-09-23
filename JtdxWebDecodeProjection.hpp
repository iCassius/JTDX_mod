#ifndef JTDX_WEB_DECODE_PROJECTION_HPP
#define JTDX_WEB_DECODE_PROJECTION_HPP

#include "MessageClient.hpp"

#include <QStringList>

// Decode the same fixed-column line that MainWindow sends to DisplayText, then
// publish through MessageClient so Web state observes the production signal.
class JtdxWebDecodeProjection
{
public:
  static bool publish (MessageClient * client, bool is_new, QString const& formatted_decode,
                       QString const& callsign, QString const& grid, bool off_air)
  {
    if (!client) return false;
    QString const decode = formatted_decode.trimmed ();
    QStringList const initial_fields = decode.left (22).split (' ', Qt::SkipEmptyParts);
    if (initial_fields.isEmpty ()) return false;
    bool const has_seconds = initial_fields.at (0).size () > 4;
    int const message_start = has_seconds ? 23 : 21;
    QStringList const fields = decode.left (message_start).split (' ', Qt::SkipEmptyParts);
    if (fields.size () < 5) return false;
    QString const message = decode.mid (message_start, 24).trimmed ();
    bool const low_confidence = decode.mid (message_start + 24, 1) == QLatin1String ("*")
        || decode.mid (message_start + 24, 1) == QLatin1String ("^");

    client->decode (is_new, QTime::fromString (fields.at (0), has_seconds ? "hhmmss" : "hhmm"),
                    fields.at (1).toInt (), fields.at (2).toFloat (), fields.at (3).toUInt (),
                    fields.at (4), message, low_confidence, off_air, callsign, grid);
    return true;
  }
};

#endif
