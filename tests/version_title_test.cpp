#include "revision_utils.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

#include "VersionInfo_jtdx.h"
#include "build_info.h"

int main (int argc, char * argv[])
{
  QCoreApplication application {argc, argv};
  application.setApplicationName (QStringLiteral ("JTDX"));
  application.setApplicationVersion (version ());

  QString const expected_version {QStringLiteral ("2.2.159.029")};
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
  if (PRODUCT_VERSION_MAJOR != 2 || PRODUCT_VERSION_MINOR != 2
      || PRODUCT_VERSION_PATCH != 159 || PRODUCT_VERSION_TWEAK != 29)
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
  qInfo () << "windows_pe_numeric_version=2.2.159.29";
  qInfo () << "window_title=" << title;
  return 0;
}
