#ifndef JTDX_LOCAL_LOG_HPP
#define JTDX_LOCAL_LOG_HPP

#include <QDir>

#include <QtGlobal>

namespace JtdxLocalLog
{
  struct Limits
  {
    qint64 max_bytes {256 * 1024};
    int rotated_files {1};
  };

  // Appends one sanitized UTF-8 line. Write failures and internal logging
  // exceptions return false; callers must not use the result as a control-flow
  // or safety decision.
  bool append (QDir const& directory, QString const& file_name,
               QString area, QString message, Limits limits = {});
}

#endif
