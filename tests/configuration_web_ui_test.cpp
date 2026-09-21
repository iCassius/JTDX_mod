#include "Configuration.hpp"
#include "MetaDataRegistry.hpp"
#include "commons.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>

#include <cstdio>
#include <functional>

// These are normally owned by MainWindow. The isolated Configuration test
// supplies the same storage so no MainWindow or decoder process is started.
int volatile itone[NUM_WSPR_SYMBOLS] {};
int volatile icw[NUM_CW_SYMBOLS] {};
dec_data_t dec_data {};

namespace
{
  int failures {0};

  void check (bool condition, char const * message)
  {
    if (!condition)
      {
        std::fprintf (stderr, "FAIL: %s\n", message);
        ++failures;
      }
  }

  QDialog * configuration_dialog ()
  {
    for (auto * widget : QApplication::topLevelWidgets ())
      {
        if (auto * dialog = qobject_cast<QDialog *> (widget))
          {
            if (dialog->objectName () == QStringLiteral ("configuration_dialog"))
              return dialog;
          }
      }
    return nullptr;
  }

  int run_dialog (Configuration& configuration,
                  std::function<void (QDialog *)> const& action)
  {
    QTimer deadline {&configuration};
    deadline.setSingleShot (true);
    QObject::connect (&deadline, &QTimer::timeout, &configuration, [&configuration] {
      if (configuration.is_active ())
        {
          check (false, "Configuration dialog interaction must finish before the deadline");
          if (auto * dialog = configuration_dialog ()) dialog->reject ();
        }
    });
    deadline.start (5000);
    QTimer::singleShot (0, [&configuration, action] {
      auto * dialog = configuration_dialog ();
      check (dialog != nullptr, "real Configuration dialog must be discoverable");
      if (dialog) action (dialog);
    });
    auto const result = configuration.exec ();
    deadline.stop ();
    return result;
  }

  void set_safe_rig_defaults (QSettings& settings)
  {
    settings.beginGroup (QStringLiteral ("Configuration"));
    settings.setValue ("Rig", QStringLiteral ("None"));
    settings.setValue ("CATTCIPort", QString {});
    settings.setValue ("CATNetworkPort", QString {});
    settings.setValue ("CATUSBPort", QString {});
    settings.setValue ("CATSerialPort", QString {});
    settings.setValue ("PTTport", QString {});
    settings.setValue ("PTTMethod", QVariant::fromValue (TransceiverFactory::PTT_method_VOX));
    settings.setValue ("QuickCall", false);
    settings.setValue ("AutoSequence", false);
    settings.setValue ("WebUiEnabled", false);
    settings.setValue ("WebUiAutomaticPort", true);
    settings.setValue ("WebUiBindAddress", QStringLiteral ("127.0.0.1"));
    settings.setValue ("WebUiAllowLan", false);
    settings.setValue ("WebUiFrequencyControlEnabled", false);
    settings.setValue ("WebUiDxControlEnabled", false);
    settings.setValue ("WebUiAutomationControlEnabled", false);
    settings.setValue ("WebUiRadioControlEnabled", false);
    settings.setValue ("WebUiPort", 49200);
    settings.remove ("WebUiToken");
    settings.remove ("WebUiTokenSha256");
    settings.sync ();
    settings.endGroup ();
    settings.sync ();
  }

  QVariantMap settings_snapshot (QSettings& settings)
  {
    QVariantMap result;
    for (auto const& key : settings.allKeys ()) result.insert (key, settings.value (key));
    return result;
  }

  void configure_and_accept (Configuration& configuration, QSettings& settings)
  {
    auto result = run_dialog (configuration, [&settings] (QDialog * dialog) {
      dialog->findChild<QCheckBox *> ("web_ui_enabled_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_frequency_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_dx_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_automation_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_radio_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_automatic_port_check_box")->setChecked (false);
      dialog->findChild<QSpinBox *> ("web_ui_port_spin_box")->setValue (49201);
      dialog->findChild<QDialogButtonBox *> ("configuration_dialog_button_box")
        ->button (QDialogButtonBox::Ok)->click ();
    });
    check (result == QDialog::Accepted, "Web UI settings must be accepted");
    check (configuration.web_ui_enabled (), "accepted Web UI enabled state is live");
    check (configuration.web_ui_frequency_control_enabled (), "accepted frequency control gate is live");
    check (configuration.web_ui_dx_control_enabled (), "accepted DX control gate is live");
    check (configuration.web_ui_automation_control_enabled (), "accepted automation control gate is live");
    check (configuration.web_ui_radio_control_enabled (), "accepted radio control gate is live");
    check (!configuration.web_ui_automatic_port (), "accepted manual port mode is live");
    check (configuration.web_ui_port () == 49201, "accepted Web UI port is live");
    check (configuration.web_ui_bind_address () == QStringLiteral ("127.0.0.1"), "default bind stays loopback");
    check (configuration_dialog () == nullptr
               || configuration_dialog ()->findChild<QWidget *> ("web_ui_token_line_edit") == nullptr,
           "Web UI settings no longer expose a token editor");
    settings.beginGroup (QStringLiteral ("Configuration"));
    check (!settings.contains ("WebUiTokenSha256") && !settings.contains ("WebUiToken"),
           "Web UI settings do not write legacy token keys");
    settings.endGroup ();
  }
}

int main (int argc, char ** argv)
{
  QApplication application {argc, argv};
  application.setApplicationName (QStringLiteral ("JTDX-P3-Configuration-WebUi-Test-%1")
                                  .arg (QCoreApplication::applicationPid ()));
  QStandardPaths::setTestModeEnabled (true);
  register_types ();

  QTemporaryDir temporary;
  check (temporary.isValid (), "temporary INI directory must be available");
  if (!temporary.isValid ()) return 1;

  QString const settings_path = temporary.filePath (QStringLiteral ("isolated.ini"));
  {
    QSettings settings {settings_path, QSettings::IniFormat};
    set_safe_rig_defaults (settings);
    {
      JTDXDateTime jtdxtime;
      Configuration configuration {&settings};
      configuration.set_jtdxtime (&jtdxtime);

    check (!configuration.web_ui_enabled (), "Web UI is disabled by default");
    check (!configuration.web_ui_frequency_control_enabled (), "frequency control is disabled by default");
    check (!configuration.web_ui_dx_control_enabled (), "DX control is disabled by default");
    check (!configuration.web_ui_automation_control_enabled (), "automation control is disabled by default");
    check (!configuration.web_ui_radio_control_enabled (), "radio control is disabled by default");
    check (configuration.web_ui_automatic_port (), "automatic port is enabled by default");
    check (configuration.web_ui_bind_address () == QStringLiteral ("127.0.0.1"), "default bind is loopback");
    check (configuration.rig_name () == QStringLiteral ("None"), "isolated configuration uses Rig=None");
    check (!configuration.is_transceiver_online (), "Rig=None starts without CAT online");

    auto const settings_before_cancel = settings_snapshot (settings);
    auto result = run_dialog (configuration, [] (QDialog * dialog) {
      dialog->findChild<QCheckBox *> ("web_ui_enabled_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_frequency_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_dx_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_automation_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_radio_control_check_box")->setChecked (true);
      dialog->findChild<QCheckBox *> ("web_ui_automatic_port_check_box")->setChecked (false);
      dialog->findChild<QSpinBox *> ("web_ui_port_spin_box")->setValue (49202);
      dialog->findChild<QDialogButtonBox *> ("configuration_dialog_button_box")
        ->button (QDialogButtonBox::Cancel)->click ();
    });
    check (result == QDialog::Rejected, "Cancel closes the real Configuration dialog");
    check (!configuration.web_ui_enabled (), "Cancel does not publish enabled state");
    check (!configuration.web_ui_frequency_control_enabled (), "Cancel does not publish frequency control gate");
    check (!configuration.web_ui_dx_control_enabled (), "Cancel does not publish DX control gate");
    check (!configuration.web_ui_automation_control_enabled (), "Cancel does not publish automation control gate");
    check (!configuration.web_ui_radio_control_enabled (), "Cancel does not publish radio control gate");
    check (configuration.web_ui_automatic_port (), "Cancel does not publish manual port mode");
    settings.sync ();
    auto const settings_after_cancel = settings_snapshot (settings);
    auto settings_before_cancel_without_geometry = settings_before_cancel;
    auto settings_after_cancel_without_geometry = settings_after_cancel;
    settings_before_cancel_without_geometry.remove (QStringLiteral ("Configuration/window/geometry"));
    settings_after_cancel_without_geometry.remove (QStringLiteral ("Configuration/window/geometry"));
    check (settings_after_cancel_without_geometry == settings_before_cancel_without_geometry,
           "Cancel does not write temporary configuration settings");

    configure_and_accept (configuration, settings);

    result = run_dialog (configuration, [] (QDialog * dialog) {
      dialog->findChild<QCheckBox *> ("web_ui_enabled_check_box")->setChecked (false);
      dialog->findChild<QCheckBox *> ("web_ui_automatic_port_check_box")->setChecked (true);
      dialog->findChild<QDialogButtonBox *> ("configuration_dialog_button_box")
        ->button (QDialogButtonBox::Cancel)->click ();
    });
    check (result == QDialog::Rejected, "repeated Configuration open can be cancelled");
    check (configuration.web_ui_enabled (), "repeated open preserves accepted settings after Cancel");
    check (!configuration.web_ui_automatic_port () && configuration.web_ui_port () == 49201,
           "repeated open preserves accepted manual port");
  }
  settings.sync ();
  }
  QSettings reloaded_settings {settings_path, QSettings::IniFormat};
  Configuration reloaded {&reloaded_settings};
  check (reloaded.web_ui_enabled (), "accepted Web UI enabled state reloads");
  check (!reloaded.web_ui_automatic_port () && reloaded.web_ui_port () == 49201,
         "accepted manual port reloads");
    check (reloaded.web_ui_frequency_control_enabled (), "accepted frequency control gate reloads");
  check (reloaded.web_ui_dx_control_enabled (), "accepted DX control gate reloads");
  check (reloaded.web_ui_automation_control_enabled (), "accepted automation control gate reloads");
  check (reloaded.web_ui_radio_control_enabled (), "accepted radio control gate reloads");
  check (reloaded.rig_name () == QStringLiteral ("None") && !reloaded.is_transceiver_online (),
         "reloaded isolated configuration remains CAT offline");

  QSettings invalid {temporary.filePath (QStringLiteral ("invalid-port.ini")), QSettings::IniFormat};
  set_safe_rig_defaults (invalid);
  invalid.beginGroup (QStringLiteral ("Configuration"));
  invalid.setValue ("WebUiEnabled", true);
  invalid.setValue ("WebUiPort", 77881);
  invalid.endGroup ();
  invalid.sync ();
  Configuration invalid_configuration {&invalid};
  check (invalid_configuration.web_ui_port () == 49200,
         "out-of-range persisted port is rejected before quint16 narrowing");
  check (!invalid_configuration.web_ui_enabled (),
         "out-of-range persisted port fails closed with Web UI disabled");

  return failures ? 1 : 0;
}
