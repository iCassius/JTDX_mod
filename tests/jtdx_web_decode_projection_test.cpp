#include "JtdxWebDecodeProjection.hpp"
#include "JtdxWebState.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cstdlib>
#include <iostream>

namespace
{
  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }
}

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  MessageClient client {QStringLiteral ("decode-test"), QStringLiteral ("test"), QString {}, 0, nullptr, false};
  JtdxWebState state {QStringLiteral ("JTDX"), QStringLiteral ("test"), QStringLiteral ("decode-test")};
  QObject::connect (&client, &MessageClient::decode_observed, &state,
                    [&state] (bool is_new, QTime time, qint32 snr, float delta_time,
                              quint32 delta_frequency, QString const& mode, QString const& message,
                              bool low_confidence, bool off_air, QString const& callsign,
                              QString const& grid) {
                      state.observe_decode (is_new, time, snr, delta_time, delta_frequency,
                                            mode, message, low_confidence, off_air, callsign, grid,
                                            QStringLiteral ("United States"), {}, QStringLiteral ("NA"));
                    });

  QString const message = QStringLiteral ("CQ W1ABC FN31 -10").leftJustified (24, QLatin1Char (' '));
  QString const timed_header = QStringLiteral ("123456 -10 0.1 1500 FT8");
  check (timed_header.size () == 23, "fixture uses the 23-column seconds layout");
  check (JtdxWebDecodeProjection::publish (&client, true, timed_header + message + QLatin1Char ('*'),
                                           QStringLiteral ("W1ABC"), QStringLiteral ("FN31"), false),
         "seconds decode projects through MessageClient");

  auto decodes = state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ();
  check (decodes.size () == 1, "MessageClient observation reaches Web state");
  auto decode = decodes.first ().toObject ();
  check (decode.value (QStringLiteral ("message")).toString () == message.trimmed (),
         "all 24 fixed-width message characters survive projection");
  check (decode.value (QStringLiteral ("time")).toString () == QStringLiteral ("12:34:56"),
         "seconds timestamp is preserved");
  check (decode.value (QStringLiteral ("snr")).toInt () == -10
             && qAbs (decode.value (QStringLiteral ("delta_time")).toDouble () - 0.1) < 0.001
             && decode.value (QStringLiteral ("delta_frequency")).toInt () == 1500
             && decode.value (QStringLiteral ("mode")).toString () == QStringLiteral ("FT8"),
         "decode measurements and mode are preserved");
  check (decode.value (QStringLiteral ("callsign")).toString () == QStringLiteral ("W1ABC")
             && decode.value (QStringLiteral ("grid")).toString () == QStringLiteral ("FN31")
             && decode.value (QStringLiteral ("low_confidence")).toBool (),
         "call, grid, and low-confidence marker are preserved");

  QString const no_seconds_header = QStringLiteral ("1234 -10 0.1 1500 FT8");
  check (no_seconds_header.size () == 21, "fixture uses the 21-column no-seconds layout");
  check (JtdxWebDecodeProjection::publish (&client, true, no_seconds_header + message,
                                           QStringLiteral ("W1ABC"), QStringLiteral ("FN31"), true),
         "no-seconds decode projects through MessageClient");
  decodes = state.json_snapshot ().value (QStringLiteral ("recent_decodes")).toArray ();
  check (decodes.size () == 2 && decodes.last ().toObject ().value (QStringLiteral ("off_air")).toBool (),
         "no-seconds time variant and off-air marker are retained");
  check (!JtdxWebDecodeProjection::publish (nullptr, true, timed_header + message,
                                            QStringLiteral ("W1ABC"), QStringLiteral ("FN31"), false),
         "missing MessageClient is rejected");
  check (!JtdxWebDecodeProjection::publish (&client, true, QStringLiteral ("malformed"), {}, {}, false),
         "malformed formatted decode is rejected");
  return 0;
}
