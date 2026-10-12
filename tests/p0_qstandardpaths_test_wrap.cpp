#include "p0_qstandardpaths_test_wrap.hpp"

#include <QCoreApplication>
#include <QDir>

#include <process.h>

#ifndef P0_TX_QSTANDARDPATHS_ALLOWED_ROOT
# error This test-only wrapper requires the local-support path root.
#endif

#if !defined(__MINGW32__)
# error The P0 test wrapper currently requires the reviewed MinGW import-pointer ABI.
#endif

using WritableLocationFunction = QString (*) (QStandardPaths::StandardLocation);

namespace
{
  QString p0_wrapped_location (QStandardPaths::StandardLocation location);
}

extern "C"
{
  extern WritableLocationFunction p0_qstandardpaths_real_import
    __asm__ ("__real___imp__ZN14QStandardPaths16writableLocationENS_16StandardLocationE");
  WritableLocationFunction p0_qstandardpaths_wrapped_import
    __asm__ ("__wrap___imp__ZN14QStandardPaths16writableLocationENS_16StandardLocationE")
    = &p0_wrapped_location;
}

namespace
{
  QString p0_wrapped_location (QStandardPaths::StandardLocation location)
  {
    QString subdirectory;
    switch (location)
      {
      case QStandardPaths::AppLocalDataLocation:
        subdirectory = QStringLiteral ("data");
        break;
      case QStandardPaths::AppDataLocation:
        subdirectory = QStringLiteral ("appdata");
        break;
      case QStandardPaths::TempLocation:
        subdirectory = QStringLiteral ("temp");
        break;
      case QStandardPaths::CacheLocation:
        subdirectory = QStringLiteral ("cache");
        break;
      case QStandardPaths::GenericDataLocation:
        subdirectory = QStringLiteral ("generic-data");
        break;
      case QStandardPaths::GenericCacheLocation:
        subdirectory = QStringLiteral ("generic-cache");
        break;
      case QStandardPaths::ConfigLocation:
        subdirectory = QStringLiteral ("config");
        break;
      case QStandardPaths::GenericConfigLocation:
        subdirectory = QStringLiteral ("generic-config");
        break;
      case QStandardPaths::AppConfigLocation:
        subdirectory = QStringLiteral ("app-config");
        break;
      case QStandardPaths::RuntimeLocation:
        subdirectory = QStringLiteral ("runtime");
        break;
      default:
        break;
      }

    if (!subdirectory.isEmpty ())
      {
        auto const process_id = QCoreApplication::applicationPid ()
          ? QCoreApplication::applicationPid () : static_cast<qint64> (_getpid ());
        auto const root = QDir::cleanPath (QStringLiteral (P0_TX_QSTANDARDPATHS_ALLOWED_ROOT));
        return QDir {root}.absoluteFilePath (
          QStringLiteral ("runs/run-%1/%2").arg (process_id).arg (subdirectory));
      }

    if (!p0_qstandardpaths_real_import) return {};
    return p0_qstandardpaths_real_import (location);
  }
}

QString p0_test_wrapped_writable_location (QStandardPaths::StandardLocation location)
{
  return p0_wrapped_location (location);
}

QString p0_test_original_writable_location (QStandardPaths::StandardLocation location)
{
  if (!p0_qstandardpaths_real_import) return {};
  return p0_qstandardpaths_real_import (location);
}
