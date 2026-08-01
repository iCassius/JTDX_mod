#include "callsignlocation.h"

#include <QRegularExpression>

namespace
{
QString provinceFor(const QString& area, const QString& suffix)
{
  struct Range { const char* first; const char* last; const char* province; };
  static const Range ranges[] = {
    { "AA", "XZ", "北京" },
    { "AA", "HZ", "黑龙江" }, { "IA", "PZ", "吉林" }, { "QA", "XZ", "辽宁" },
    { "AA", "FZ", "天津" }, { "GA", "LZ", "内蒙古" }, { "MA", "RZ", "河北" }, { "SA", "XZ", "山西" },
    { "AA", "HZ", "上海" }, { "IA", "PZ", "山东" }, { "QA", "XZ", "江苏" },
    { "AA", "HZ", "浙江" }, { "IA", "PZ", "江西" }, { "QA", "XZ", "福建" },
    { "AA", "HZ", "安徽" }, { "IA", "PZ", "河南" }, { "QA", "XZ", "湖北" },
    { "AA", "HZ", "湖南" }, { "IA", "PZ", "广东" }, { "QA", "XZ", "广西" }, { "YA", "ZZ", "海南" },
    { "AA", "FZ", "四川" }, { "GA", "LZ", "重庆" }, { "MA", "RZ", "贵州" }, { "SA", "XZ", "云南" },
    { "AA", "FZ", "陕西" }, { "GA", "LZ", "甘肃" }, { "MA", "RZ", "宁夏" }, { "SA", "XZ", "青海" },
    { "AA", "FZ", "新疆" }, { "GA", "LZ", "西藏" }
  };

  int first = 0;
  int last = 0;
  switch (area.toInt()) {
  case 1: return QString::fromUtf8("北京");
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
      return QString::fromUtf8(ranges[index].province);
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
