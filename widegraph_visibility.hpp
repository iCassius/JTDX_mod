#ifndef WIDEGRAPH_VISIBILITY_HPP
#define WIDEGRAPH_VISIBILITY_HPP

#include <QSettings>
#include <QWidget>

namespace WideGraphVisibility
{
  constexpr char const * settingsKey = "WideGraph/visible";

  inline bool shouldRestore (QSettings const& settings)
  {
    return settings.value (settingsKey, true).toBool ();
  }

  inline void restore (QWidget& window, QSettings const& settings)
  {
    if (shouldRestore (settings))
      {
        window.setAttribute (Qt::WA_ShowWithoutActivating);
        window.show ();
        window.setAttribute (Qt::WA_ShowWithoutActivating, false);
      }
  }

  inline void openExplicitly (QWidget& window, QSettings& settings)
  {
    settings.setValue (settingsKey, true);
    window.show ();
  }

  inline void recordClosed (QSettings& settings, bool ownerIsRunning)
  {
    if (ownerIsRunning) settings.setValue (settingsKey, false);
  }
}

#endif // WIDEGRAPH_VISIBILITY_HPP
