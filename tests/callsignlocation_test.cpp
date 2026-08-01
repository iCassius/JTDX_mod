#include "logbook/callsignlocation.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

namespace
{
void expectEqual(const QString& actual, const QString& expected, const char* description)
{
  if (actual != expected) {
    std::cerr << description << ": expected '" << expected.toStdString()
              << "', got '" << actual.toStdString() << "'\n";
    std::exit(1);
  }
}
}

int main(int argc, char *argv[])
{
  QCoreApplication application(argc, argv);

  expectEqual(CallsignLocation::chinaProvince("BA3MAB", "B"), QString::fromUtf8("河北"), "BA3MAB");
  expectEqual(CallsignLocation::chinaProvince("BG0ABC", "B"), QString::fromUtf8("新疆"), "BG0ABC");
  expectEqual(CallsignLocation::chinaProvince("BA3SAB", "B"), QString::fromUtf8("山西"), "BA3SAB");
  expectEqual(CallsignLocation::chinaProvince("BA3M", "B"), "", "short suffix");
  expectEqual(CallsignLocation::chinaProvince("BA3MAB/P", "B"), "", "portable suffix");
  expectEqual(CallsignLocation::chinaProvince("VR2ABC", "B"), "", "non B-prefix call");
  expectEqual(CallsignLocation::chinaProvince("BA3MAB", "BV"), "", "non China DXCC");

  return 0;
}
