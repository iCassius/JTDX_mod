#include <QFile>
#include <QByteArray>

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool condition, char const * description)
  {
    if (!condition) {
      std::cerr << "failed: " << description << '\n';
      std::exit (1);
    }
  }
}

int main ()
{
  QFile ui {QStringLiteral (JTDX_SOURCE_DIR "/mainwindow.ui")};
  expect (ui.open (QIODevice::ReadOnly), "open mainwindow UI");
  auto const xml = ui.readAll ();
  expect (!xml.contains ("<height>7</height>"), "button maximum height cannot collapse to seven pixels");
  expect (xml.contains ("name=\"AutoSeqButton\""), "AutoSeq button exists");
  expect (xml.contains ("name=\"AutoTxButton\""), "AutoTx button exists");
  expect (xml.contains ("name=\"stopTxButton\""), "Halt Tx button exists");
  expect (xml.contains ("自动起呼新呼号"), "new callsign menu text exists");
  expect (xml.contains ("自动起呼新呼号波段"), "new callsign band menu text exists");
  return 0;
}
