#include "callsignlocation.h"

#include <QRegularExpression>

namespace
{
QString provinceFor(const QString& area, const QString& suffix)
{
  struct Range { const char* first; const char* last; const char* province; };
  static const Range ranges[] = {
    { "AA", "XZ", "Beijing" },
    { "AA", "HZ", "Heilongjiang" }, { "IA", "PZ", "Jilin" }, { "QA", "XZ", "Liaoning" },
    { "AA", "FZ", "Tianjin" }, { "GA", "LZ", "Inner Mongolia" }, { "MA", "RZ", "Hebei" }, { "SA", "XZ", "Shanxi" },
    { "AA", "HZ", "Shanghai" }, { "IA", "PZ", "Shandong" }, { "QA", "XZ", "Jiangsu" },
    { "AA", "HZ", "Zhejiang" }, { "IA", "PZ", "Jiangxi" }, { "QA", "XZ", "Fujian" },
    { "AA", "HZ", "Anhui" }, { "IA", "PZ", "Henan" }, { "QA", "XZ", "Hubei" },
    { "AA", "HZ", "Hunan" }, { "IA", "PZ", "Guangdong" }, { "QA", "XZ", "Guangxi" }, { "YA", "ZZ", "Hainan" },
    { "AA", "FZ", "Sichuan" }, { "GA", "LZ", "Chongqing" }, { "MA", "RZ", "Guizhou" }, { "SA", "XZ", "Yunnan" },
    { "AA", "FZ", "Shaanxi" }, { "GA", "LZ", "Gansu" }, { "MA", "RZ", "Ningxia" }, { "SA", "XZ", "Qinghai" },
    { "AA", "FZ", "Xinjiang" }, { "GA", "LZ", "Tibet" }
  };

  int first = 0;
  int last = 0;
  switch (area.toInt()) {
  case 1: return CallsignLocation::tr("Beijing");
  case 2: first = 1;  last = 3;  break;
  case 3: first = 4;  last = 7;  break;
  case 4: first = 8;  last = 10; break;
  case 5: first = 11; last = 13; break;
  case 6: first = 14; last = 16; break;
  case 7: first = 17; last = 20; break;
  case 8: first = 21; last = 24; break;
  case 9: first = 25; last = 28; break;
  case 0: first = 29; last = 30; break;
  default: return QString();
  }

  const QString allocation = suffix.left(2);
  for (int index = first; index <= last; ++index) {
    if (allocation >= QLatin1String(ranges[index].first) && allocation <= QLatin1String(ranges[index].last)) {
      return CallsignLocation::tr(ranges[index].province);
    }
  }
  return QString();
}
}

QString CallsignLocation::chinaProvince(const QString& callsign, const QString& masterPrefix)
{
  // Keep the effective pattern intentionally identical to the installed
  // TX-5DR build: only ordinary B-prefixed 2/3-letter suffix calls qualify.
  if (masterPrefix.toUpper() != QLatin1String("B")) {
    return QString();
  }
  static const QRegularExpression pattern(
      QLatin1String("^B[GHIDABCEFKL]([0-9])([A-Z]{2,3})$"));
  const QRegularExpressionMatch match = pattern.match(callsign.trimmed().toUpper());
  if (!match.hasMatch()) {
    return QString();
  }
  return provinceFor(match.captured(1), match.captured(2));
}
