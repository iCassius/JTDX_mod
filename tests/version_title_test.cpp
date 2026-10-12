#include "revision_utils.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

#include "VersionInfo_jtdx.h"
#include "build_info.h"

#ifndef JTDX_EXPECTED_VERSION_STRING
#error "version_title_test requires its expected display version from CMake"
#endif
#ifndef JTDX_EXPECTED_PE_MAJOR
#error "version_title_test requires its expected PE major version from CMake"
#endif
#ifndef JTDX_EXPECTED_PE_MINOR
#error "version_title_test requires its expected PE minor version from CMake"
#endif
#ifndef JTDX_EXPECTED_PE_PATCH
#error "version_title_test requires its expected PE patch version from CMake"
#endif
#ifndef JTDX_EXPECTED_PE_TWEAK
#error "version_title_test requires its expected PE tweak version from CMake"
#endif

int main (int argc, char * argv[])
{
  QCoreApplication application {argc, argv};
  application.setApplicationName (QStringLiteral ("JTDX"));
  application.setApplicationVersion (version ());

  QString const expected_version {QStringLiteral (JTDX_EXPECTED_VERSION_STRING)};
  QString const expected_pe_version {
    QString::number (JTDX_EXPECTED_PE_MAJOR) + QLatin1Char ('.')
    + QString::number (JTDX_EXPECTED_PE_MINOR) + QLatin1Char ('.')
    + QString::number (JTDX_EXPECTED_PE_PATCH) + QLatin1Char ('.')
    + QString::number (JTDX_EXPECTED_PE_TWEAK)};
  QString const build_timestamp {QString::fromUtf8 (JTDX_BUILD_TIMESTAMP)};
  QString const title = program_title ();
  if (version () != expected_version)
    {
      qCritical () << "Unexpected application version:" << version ();
      return 1;
    }
  if (application.applicationVersion () != expected_version)
    {
      qCritical () << "QCoreApplication version differs:" << application.applicationVersion ();
      return 1;
    }
  if (PRODUCT_VERSION_MAJOR != JTDX_EXPECTED_PE_MAJOR
      || PRODUCT_VERSION_MINOR != JTDX_EXPECTED_PE_MINOR
      || PRODUCT_VERSION_PATCH != JTDX_EXPECTED_PE_PATCH
      || PRODUCT_VERSION_TWEAK != JTDX_EXPECTED_PE_TWEAK)
    {
      qCritical () << "Unexpected numeric Windows PE version components";
      return 1;
    }
  if (build_timestamp.isEmpty () || !title.startsWith (QStringLiteral ("JTDX v") + expected_version)
      || !title.endsWith (QStringLiteral (" — Build ") + build_timestamp))
    {
      qCritical () << "Build timestamp/title contract failed:" << title;
      return 1;
    }
  qInfo () << "application_version=" << application.applicationVersion ();
  qInfo () << "windows_pe_numeric_version=" << expected_pe_version;
  qInfo () << "window_title=" << title;
  return 0;
}
