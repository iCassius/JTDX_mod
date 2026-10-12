#include "p0_qstandardpaths_test_wrap.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstdio>

#ifndef P0_TX_QSTANDARDPATHS_ALLOWED_ROOT
# error This preflight requires the local-support path root.
#endif

namespace
{
  bool is_within_root (QString const& path, QString const& root)
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

  bool check_location (QStandardPaths::StandardLocation location,
                       char const * name, QString const& allowed_root)
  {
    auto const path = QStandardPaths::writableLocation (location);
    std::fprintf (stderr, "PREFLIGHT location=%s path=%s\n", name, qPrintable (path));
    return !path.isEmpty () && QDir::isAbsolutePath (path)
      && is_within_root (path, allowed_root);
  }
}

int main (int argc, char ** argv)
{
  QStandardPaths::setTestModeEnabled (true);
  QCoreApplication app (argc, argv);
  app.setOrganizationName (QStringLiteral ("JTDX-P0-S4.2"));
  app.setApplicationName (QStringLiteral ("QStandardPaths-preflight"));

  auto const root = QDir::cleanPath (QStringLiteral (P0_TX_QSTANDARDPATHS_ALLOWED_ROOT));
  auto const data = QStandardPaths::writableLocation (QStandardPaths::DataLocation);
  auto const app_local = QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation);
  auto const app_data = QStandardPaths::writableLocation (QStandardPaths::AppDataLocation);
  auto const temp = QStandardPaths::writableLocation (QStandardPaths::TempLocation);
  std::fprintf (stderr, "PREFLIGHT root=%s\n", qPrintable (root));
  std::fprintf (stderr, "PREFLIGHT location=DataLocation path=%s\n", qPrintable (data));
  std::fprintf (stderr, "PREFLIGHT location=AppLocalDataLocation path=%s\n", qPrintable (app_local));
  std::fprintf (stderr, "PREFLIGHT location=AppDataLocation path=%s\n", qPrintable (app_data));
  std::fprintf (stderr, "PREFLIGHT location=TempLocation path=%s\n", qPrintable (temp));

  bool ok = !data.isEmpty () && data == app_local
    && is_within_root (data, root) && is_within_root (app_data, root)
    && is_within_root (temp, root);
  ok &= check_location (QStandardPaths::CacheLocation, "CacheLocation", root);
  ok &= check_location (QStandardPaths::ConfigLocation, "ConfigLocation", root);
  ok &= check_location (QStandardPaths::AppConfigLocation, "AppConfigLocation", root);

  auto const documents = QStandardPaths::writableLocation (QStandardPaths::DocumentsLocation);
  auto const original_documents = p0_test_original_writable_location (QStandardPaths::DocumentsLocation);
  std::fprintf (stderr, "PREFLIGHT readonly_delegation DocumentsLocation wrapped=%s original=%s\n",
                qPrintable (documents), qPrintable (original_documents));
  ok &= !documents.isEmpty () && documents == original_documents;

  if (!ok)
    {
      std::fprintf (stderr, "FAIL: QStandardPaths test wrapper did not keep all write locations inside the fixed local-support root or did not delegate read-only locations.\n");
      return 1;
    }

  std::fprintf (stderr, "PASS: no MainWindow constructed; writable path wrappers are inside local-support and DocumentsLocation delegates to Qt. No directory or file was created by this preflight.\n");
  return 0;
}
