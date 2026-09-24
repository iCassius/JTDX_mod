#include "widegraph_visibility.hpp"

#include <QApplication>
#include <QDialog>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }
}

int main (int argc, char * argv[])
{
  QApplication app {argc, argv};
  QTemporaryDir directory;
  expect (directory.isValid (), "create isolated settings directory");
  QSettings settings {directory.filePath ("widegraph.ini"), QSettings::IniFormat};
  QDialog window;

  expect (WideGraphVisibility::shouldRestore (settings),
          "missing visibility setting preserves first-run visible default");
  WideGraphVisibility::restore (window, settings);
  app.processEvents ();
  expect (window.isVisible (), "visible intent restores the standalone window");
  expect (!window.testAttribute (Qt::WA_ShowWithoutActivating),
          "startup-only no-activation attribute is reset after show");

  WideGraphVisibility::recordClosed (settings, true);
  settings.sync ();
  expect (!WideGraphVisibility::shouldRestore (settings),
          "a runtime close persists hidden intent");

  WideGraphVisibility::recordClosed (settings, false);
  settings.sync ();
  expect (!WideGraphVisibility::shouldRestore (settings),
          "owner shutdown cleanup does not overwrite the last user choice");

  window.hide ();
  WideGraphVisibility::restore (window, settings);
  app.processEvents ();
  expect (!window.isVisible (), "hidden intent stays hidden during restoration");

  WideGraphVisibility::openExplicitly (window, settings);
  app.processEvents ();
  expect (window.isVisible (), "explicit open displays the window");
  expect (WideGraphVisibility::shouldRestore (settings),
          "explicit open persists visible intent");

  window.close ();
  return 0;
}
