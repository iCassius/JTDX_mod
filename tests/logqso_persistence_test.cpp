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

  bool is_within_path (QString const& path, QString const& root)
  {
    auto const normalized_path = QDir::cleanPath (QDir::fromNativeSeparators (path));
    auto const normalized_root = QDir::cleanPath (QDir::fromNativeSeparators (root));
    auto const prefix = normalized_root.endsWith (QLatin1Char ('/'))
      ? normalized_root : normalized_root + QLatin1Char ('/');
#ifdef Q_OS_WIN
    return normalized_path.compare (normalized_root, Qt::CaseInsensitive) == 0
      || normalized_path.startsWith (prefix, Qt::CaseInsensitive);
#else
    return normalized_path == normalized_root || normalized_path.startsWith (prefix);
#endif
  }
}

int main (int argc, char ** argv)
{
  QApplication application {argc, argv};
  application.setApplicationName (QStringLiteral ("JTDX-P19-LogQSO-Test"));
  QString data_path;
  QString owned_run_root;
#ifdef P19_LOGQSO_ALLOWED_ROOT
  QStandardPaths::setTestModeEnabled (true);
  auto const allowed_root = QDir::cleanPath (
    QStringLiteral (P19_LOGQSO_ALLOWED_ROOT));
  data_path = QStandardPaths::writableLocation (QStandardPaths::DataLocation);
  owned_run_root = QDir {allowed_root}.absoluteFilePath (
    QStringLiteral ("runs/run-%1").arg (application.applicationPid ()));
  auto const expected_path = QDir {owned_run_root}.absoluteFilePath (
    QStringLiteral ("data"));
  auto const normalized_run_root = QDir::cleanPath (
    QDir::fromNativeSeparators (owned_run_root));
  auto const normalized_allowed_root = QDir::cleanPath (
    QDir::fromNativeSeparators (allowed_root));
#ifdef Q_OS_WIN
  bool const run_root_strictly_inside = is_within_path (owned_run_root, allowed_root)
    && normalized_run_root.compare (normalized_allowed_root, Qt::CaseInsensitive) != 0;
  bool const owned_path_matches = QDir::cleanPath (QDir::fromNativeSeparators (data_path))
    .compare (QDir::cleanPath (QDir::fromNativeSeparators (expected_path)), Qt::CaseInsensitive) == 0;
#else
  bool const run_root_strictly_inside = is_within_path (owned_run_root, allowed_root)
    && normalized_run_root != normalized_allowed_root;
  bool const owned_path_matches = QDir::cleanPath (QDir::fromNativeSeparators (data_path))
    == QDir::cleanPath (QDir::fromNativeSeparators (expected_path));
#endif
  if (!QDir::isAbsolutePath (allowed_root) || data_path.isEmpty ()
      || !QDir::isAbsolutePath (data_path)
      || !run_root_strictly_inside
      || !is_within_path (data_path, allowed_root) || !owned_path_matches)
    {
      std::fprintf (stderr, "FAIL DataLocation is outside the test-owned local-support root: %s\n",
                    data_path.toUtf8 ().constData ());
      return 2;
    }
  if (QDir {owned_run_root}.exists ())
    {
      std::fprintf (stderr, "FAIL test-owned LogQSO run directory already exists; leaving it untouched: %s\n",
                    QDir {owned_run_root}.absolutePath ().toUtf8 ().constData ());
      return 2;
    }
#else
  QStandardPaths::setTestModeEnabled (true);
  data_path = QStandardPaths::writableLocation (QStandardPaths::DataLocation);
#endif

  QDir data_dir {data_path};
#ifdef P19_LOGQSO_ALLOWED_ROOT
  if (data_dir.path ().isEmpty () || !data_dir.mkpath (QStringLiteral (".")))
    {
      std::fprintf (stderr, "FAIL cannot create isolated Qt test data directory: %s\n",
                    data_dir.absolutePath ().toUtf8 ().constData ());
      return 2;
    }
#else
  if (data_dir.exists ()) data_dir.removeRecursively ();
  check (!data_dir.path ().isEmpty () && data_dir.mkpath (QStringLiteral (".")),
         "isolated Qt test data directory is writable");
#endif
  QString const adif_path = data_dir.absoluteFilePath (QStringLiteral ("wsjtx_log.adi"));
  QString const log_path = data_dir.absoluteFilePath (QStringLiteral ("wsjtx.log"));
  QFile::remove (adif_path);
  QFile::remove (log_path);

  QString const settings_path = data_dir.absoluteFilePath (QStringLiteral ("p19-settings.ini"));
  auto const start = QDateTime::fromString (QStringLiteral ("2026-09-21T10:03:00"), Qt::ISODate);
  auto const end = QDateTime::fromString (QStringLiteral ("2026-09-21T10:03:15"), Qt::ISODate);
  QString reason;
  QByteArray first_adif;
  QByteArray first_log;
  qint64 final_log_bytes {0};
  {
    QSettings settings {settings_path, QSettings::IniFormat};
    set_safe_settings (settings);
    Configuration configuration {&settings};
    JTDXDateTime jtdx_time;
    LogQSO log_qso {&settings, &configuration, &jtdx_time};

    log_qso.initWebLogQSO (QStringLiteral ("K1P19"), QStringLiteral ("FN31"), QStringLiteral ("FT8"),
                           QStringLiteral ("-10"), QStringLiteral ("-09"), QString {}, QStringLiteral ("P19"),
                           start, end, 14074000, QStringLiteral ("50W"), QStringLiteral ("isolated"), QString {});
    check (log_qso.acceptWebQSO (&reason), "real LogQSO confirmation reports success");
    first_adif = read_file (adif_path);
    first_log = read_file (log_path);
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
    // ADIF's QFile::open() failure within this fixture's owned run directory.
    check (QDir {adif_path}.mkpath (QStringLiteral (".")),
           "blocked-path fixture replaces only the isolated ADIF path with a directory");
    reason.clear ();
    check (!log_qso.acceptWebQSO (&reason) && !reason.isEmpty (),
           "real LogQSO write failure is returned instead of reported as success");
    check (read_file (log_path).isEmpty (),
           "failed ADIF write emits no business-log success record");
    final_log_bytes = read_file (log_path).size ();
    QDir {adif_path}.removeRecursively ();
  }

#ifdef P19_LOGQSO_ALLOWED_ROOT
  QDir {owned_run_root}.removeRecursively ();
#else
  QFile::remove (settings_path);
  data_dir.removeRecursively ();
#endif

  std::fprintf (stdout, "P19_LOGQSO_DATA_DIR=%s\n", data_dir.absolutePath ().toUtf8 ().constData ());
  std::fprintf (stdout, "P19_LOGQSO_ADIF_RECORDS=%d\n", first_adif.count ("<eor>"));
  std::fprintf (stdout, "P19_LOGQSO_ADIF=%s\n", first_adif.constData ());
  std::fprintf (stdout, "P19_LOGQSO_ADIF_PATH_IS_DIR=%d\n", QDir {adif_path}.exists () ? 1 : 0);
  std::fprintf (stdout, "P19_LOGQSO_LOG_BYTES=%lld\n", static_cast<long long> (final_log_bytes));
  std::fprintf (stdout, "P19_LOGQSO_FAILURE_REASON=%s\n", reason.toUtf8 ().constData ());
  return failures == 0 ? 0 : 1;
}
