#include "autoseq_diagnostic_formatter.hpp"

#include <QUrl>
#include <QMap>

#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }
}

int main ()
{
  QStringList fields;
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("event"),
                                             QStringLiteral ("ptt_readback_off"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("build"),
                                             QStringLiteral ("Oct  3 2026 10:20:30"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("call"), QString {});
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("tx_text"),
                                             QStringLiteral ("CQ K1%ABC 北京"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("cleanup_target_before"),
                                             QStringLiteral ("K2OLD"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("cleanup_target_after"),
                                             QString {});
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("operator_hold_before"),
                                             QStringLiteral ("false"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("operator_hold_after"),
                                             QStringLiteral ("false"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("owner_active_before"),
                                             QStringLiteral ("true"));
  AutoSeqDiagnosticFormatter::append_field (fields, QStringLiteral ("owner_active_after"),
                                             QStringLiteral ("false"));

  auto const line = AutoSeqDiagnosticFormatter::line (fields);
  require (line.contains (QStringLiteral ("call=")),
           "an empty callsign remains an explicit empty field");
  require (line.contains (QStringLiteral ("build=Oct%20%203%202026%2010%3A20%3A30")),
           "spaces and colons in build text are escaped within its own field");
  require (line.contains (QStringLiteral ("tx_text=CQ%20K1%25ABC%20%E5%8C%97%E4%BA%AC")),
           "percent signs and Chinese text are encoded without placeholder expansion");

  QMap<QString, QString> decoded;
  for (auto const& field: line.split (QLatin1Char (' ')))
    {
      auto const equals = field.indexOf (QLatin1Char ('='));
      require (equals > 0, "each emitted token retains a key and value boundary");
      auto const key = field.left (equals);
      auto const value = QUrl::fromPercentEncoding (field.mid (equals + 1).toLatin1 ());
      require (!decoded.contains (key), "diagnostic field names remain unique");
      decoded.insert (key, value);
    }
  require (decoded.value (QStringLiteral ("event")) == QStringLiteral ("ptt_readback_off"),
           "event remains associated with its original key");
  require (decoded.value (QStringLiteral ("build")) == QStringLiteral ("Oct  3 2026 10:20:30"),
           "the build value decodes exactly after formatting");
  require (decoded.value (QStringLiteral ("call")).isEmpty (),
           "the callsign decodes as empty instead of consuming a later placeholder");
  require (decoded.value (QStringLiteral ("tx_text")) == QStringLiteral ("CQ K1%ABC 北京"),
           "percent and Chinese text decode exactly from tx_text");
  require (decoded.value (QStringLiteral ("cleanup_target_before")) == QStringLiteral ("K2OLD"),
           "cleanup target before is retained as its own field");
  require (decoded.value (QStringLiteral ("cleanup_target_after")).isEmpty (),
           "cleared target remains an explicit empty after-field");
  require (decoded.value (QStringLiteral ("operator_hold_before")) == QStringLiteral ("false")
           && decoded.value (QStringLiteral ("operator_hold_after")) == QStringLiteral ("false"),
           "operator hold before and after remain distinct fields");
  require (decoded.value (QStringLiteral ("owner_active_before")) == QStringLiteral ("true")
           && decoded.value (QStringLiteral ("owner_active_after")) == QStringLiteral ("false"),
           "stall owner before and after remain distinct fields");

  return 0;
}
