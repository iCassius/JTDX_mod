#include "Configuration.hpp"
#include "JTDXDateTime.h"
#include "commons.h"
#include "logqso.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include <cstdio>

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

  void set_safe_settings (QSettings& settings)
  {
    settings.beginGroup (QStringLiteral ("Configuration"));
    settings.setValue (QStringLiteral ("Rig"), QStringLiteral ("None"));
    settings.setValue (QStringLiteral ("CATTCIPort"), QString {});
    settings.setValue (QStringLiteral ("CATNetworkPort"), QString {});
    settings.setValue (QStringLiteral ("CATUSBPort"), QString {});
    settings.setValue (QStringLiteral ("CATSerialPort"), QString {});
    settings.setValue (QStringLiteral ("PTTport"), QString {});
    settings.setValue (QStringLiteral ("PTTMethod"), QVariant::fromValue (TransceiverFactory::PTT_method_VOX));
    settings.setValue (QStringLiteral ("MyCall"), QStringLiteral ("P19TEST"));
    settings.setValue (QStringLiteral ("MyGrid"), QStringLiteral ("OM89"));
    settings.setValue (QStringLiteral ("WebUiEnabled"), false);
    settings.sync ();
    settings.endGroup ();
    settings.sync ();
  }

  QByteArray read_file (QString const& path)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly)) return {};
    return file.readAll ();
  }
}

int main (int argc, char ** argv)
{
  QApplication application {argc, argv};
  application.setApplicationName (QStringLiteral ("JTDX-P19-LogQSO-Test"));
  QStandardPaths::setTestModeEnabled (true);

  QDir data_dir {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
  if (data_dir.exists ()) data_dir.removeRecursively ();
  data_dir = QDir {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
  check (!data_dir.isEmpty () && data_dir.mkpath (QStringLiteral (".")),
         "isolated Qt test data directory is writable");
  QString const adif_path = data_dir.absoluteFilePath (QStringLiteral ("wsjtx_log.adi"));
  QString const log_path = data_dir.absoluteFilePath (QStringLiteral ("wsjtx.log"));
  QFile::remove (adif_path);
  QFile::remove (log_path);

  QString const settings_path = data_dir.absoluteFilePath (QStringLiteral ("p19-settings.ini"));
  QSettings settings {settings_path, QSettings::IniFormat};
  set_safe_settings (settings);
  Configuration configuration {&settings};
  JTDXDateTime jtdx_time;
  LogQSO log_qso {&settings, &configuration, &jtdx_time};

  auto const start = QDateTime::fromString (QStringLiteral ("2026-09-21T10:03:00"), Qt::ISODate);
  auto const end = QDateTime::fromString (QStringLiteral ("2026-09-21T10:03:15"), Qt::ISODate);
  log_qso.initWebLogQSO (QStringLiteral ("K1P19"), QStringLiteral ("FN31"), QStringLiteral ("FT8"),
                         QStringLiteral ("-10"), QStringLiteral ("-09"), QString {}, QStringLiteral ("P19"),
                         start, end, 14074000, QStringLiteral ("50W"), QStringLiteral ("isolated"), QString {});
  QString reason;
  check (log_qso.acceptWebQSO (&reason), "real LogQSO confirmation reports success");
  QByteArray const first_adif = read_file (adif_path);
  QByteArray const first_log = read_file (log_path);
  check (first_adif.contains ("<call:5>K1P19") && first_adif.count ("<eor>") == 1,
         "real LogQSO confirmation writes exactly one ADIF record");
  check (first_log.contains ("K1P19,FN31,14.074000,FT8,-10,-09,50W,isolated,P19"),
         "real LogQSO confirmation writes the business log record");

  QFile::remove (adif_path);
  QFile::remove (log_path);
  check (!QFile::exists (adif_path) && !QFile::exists (log_path),
         "cancel fixture starts with zero persisted records");
  check (!QFile::exists (adif_path) && !QFile::exists (log_path),
         "cancel path performs no LogQSO write");

  log_qso.initWebLogQSO (QStringLiteral ("K1P19"), QStringLiteral ("FN31"), QStringLiteral ("FT8"),
                         QStringLiteral ("-10"), QStringLiteral ("-09"), QString {}, QStringLiteral ("P19"),
                         start, end, 14074000, QStringLiteral ("50W"), QStringLiteral ("blocked"), QString {});
  // The real path is DataLocation; replace the file with a directory to force
  // ADIF's QFile::open() failure without touching a user profile.
  check (QDir {adif_path}.mkpath (QStringLiteral (".")),
         "blocked-path fixture replaces only the isolated ADIF path with a directory");
  reason.clear ();
  check (!log_qso.acceptWebQSO (&reason) && !reason.isEmpty (),
         "real LogQSO write failure is returned instead of reported as success");
  check (read_file (log_path).isEmpty (),
         "failed ADIF write emits no business-log success record");
  QDir {adif_path}.removeRecursively ();
  QFile::remove (settings_path);

  std::fprintf (stdout, "P19_LOGQSO_DATA_DIR=%s\n", data_dir.absolutePath ().toUtf8 ().constData ());
  std::fprintf (stdout, "P19_LOGQSO_ADIF_RECORDS=%d\n", first_adif.count ("<eor>"));
  std::fprintf (stdout, "P19_LOGQSO_ADIF=%s\n", first_adif.constData ());
  std::fprintf (stdout, "P19_LOGQSO_ADIF_PATH_IS_DIR=%d\n", QDir {adif_path}.exists () ? 1 : 0);
  std::fprintf (stdout, "P19_LOGQSO_LOG_BYTES=%lld\n", static_cast<long long> (read_file (log_path).size ()));
  std::fprintf (stdout, "P19_LOGQSO_FAILURE_REASON=%s\n", reason.toUtf8 ().constData ());
  data_dir.removeRecursively ();
  return failures == 0 ? 0 : 1;
}
