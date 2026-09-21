#include "JtdxLocalLog.hpp"
#include "JtdxWebControl.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace
{
  void check (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  QByteArray read (QString const& path)
  {
    QFile file {path};
    check (file.open (QIODevice::ReadOnly), "diagnostic log can be read");
    return file.readAll ();
  }

  JtdxWebControl::ObservedState safe_state ()
  {
    JtdxWebControl::ObservedState state;
    state.safety.known = true;
    state.safety.fresh = true;
    state.safety.rig_online = true;
    state.safety.monitoring = true;
    state.safety.business_state_known = true;
    state.state_revision = 1;
    state.frequency_known = true;
    state.actual_frequency_hz = 14074000;
    state.frequency_generation = 1;
    return state;
  }
}

int main ()
{
  QTemporaryDir temporary;
  check (temporary.isValid (), "temporary diagnostic directory is available");
  QDir const directory {temporary.path ()};
  JtdxLocalLog::Limits limits;
  limits.max_bytes = 256;
  limits.rotated_files = 1;

  check (JtdxLocalLog::append (directory, QStringLiteral ("control.log"),
                              QStringLiteral ("web-control"),
                              QStringLiteral ("unicode-诊断\r\ninjected=field"), limits),
         "unicode diagnostic line writes");
  QByteArray current = read (directory.absoluteFilePath (QStringLiteral ("control.log")));
  check (current.contains ("unicode-\xe8\xaf\x8a\xe6\x96\xad")
             && current.contains ("injected=field")
             && current.count ('\n') == 1 && !current.contains ("\r\n"),
         "Unicode is retained and newline injection stays on one line");

  JtdxLocalLog::Limits short_limits;
  short_limits.max_bytes = 64;
  check (JtdxLocalLog::append (directory, QStringLiteral ("oversize.log"),
                               QStringLiteral ("legacy"), QString (2000, QLatin1Char ('x')),
                               short_limits),
         "overlong diagnostic line writes within the limit");
  check (QFileInfo {directory.absoluteFilePath (QStringLiteral ("oversize.log"))}.size ()
             <= short_limits.max_bytes,
         "overlong single line stays within the limit");

  QFile existing {directory.absoluteFilePath (QStringLiteral ("existing.log"))};
  check (existing.open (QIODevice::WriteOnly), "oversize existing log fixture opens");
  check (existing.write (QByteArray (400, 'e')) == 400, "oversize existing log fixture writes");
  existing.close ();
  JtdxLocalLog::Limits existing_limits;
  existing_limits.max_bytes = 128;
  check (JtdxLocalLog::append (directory, QStringLiteral ("existing.log"),
                               QStringLiteral ("legacy"), QStringLiteral ("key=value"),
                               existing_limits),
         "oversize existing log is rotated and appended");
  check (QFileInfo {directory.absoluteFilePath (QStringLiteral ("existing.log"))}.size ()
             <= existing_limits.max_bytes
             && QFileInfo {directory.absoluteFilePath (QStringLiteral ("existing.log.1"))}.size ()
             <= existing_limits.max_bytes,
         "active and rotated files stay bounded after an oversize existing file");
  check (read (directory.absoluteFilePath (QStringLiteral ("existing.log"))).contains ("key=value"),
         "legacy key-value separators remain parseable");

  QString const rotation_failure_path = directory.absoluteFilePath (QStringLiteral ("rotation-failure.log"));
  QDir const rotation_blocker {rotation_failure_path + QStringLiteral (".1")};
  check (rotation_blocker.mkpath (QStringLiteral (".")), "rotation failure blocker directory is created");
  QFile rotation_failure {rotation_failure_path};
  check (rotation_failure.open (QIODevice::WriteOnly), "rotation failure fixture opens");
  check (rotation_failure.write (QByteArray (400, 'r')) == 400, "rotation failure fixture writes");
  rotation_failure.close ();
  JtdxLocalLog::Limits rotation_failure_limits;
  rotation_failure_limits.max_bytes = 128;
  check (JtdxLocalLog::append (directory, QStringLiteral ("rotation-failure.log"),
                               QStringLiteral ("legacy"), QStringLiteral ("fallback=value"),
                               rotation_failure_limits),
         "rotation failure falls back to bounded active log");
  check (QFileInfo {rotation_failure_path}.size () <= rotation_failure_limits.max_bytes,
         "rotation failure keeps the active file bounded");

  for (int i = 0; i != 12; ++i)
    check (JtdxLocalLog::append (directory, QStringLiteral ("control.log"),
                                 QStringLiteral ("web-control"),
                                 QStringLiteral ("bounded-line-%1-%2").arg (i).arg (QString (180, QLatin1Char ('x'))),
                                 limits),
           "bounded diagnostic line writes");
  QFileInfo const current_info {directory.absoluteFilePath (QStringLiteral ("control.log"))};
  QFileInfo const rotated_info {directory.absoluteFilePath (QStringLiteral ("control.log.1"))};
  check (current_info.size () <= limits.max_bytes && rotated_info.size () <= limits.max_bytes,
         "active and rotated logs stay within the byte limit");
  check (!QFileInfo::exists (directory.absoluteFilePath (QStringLiteral ("control.log.2"))),
         "rotation is limited to one file");

  JtdxWebControl control {100};
  control.bind_server_epoch (control.server_epoch ());
  control.set_clock_for_test (0);
  control.set_observed_state (safe_state ());
  int dispatch_count = 0;
  control.set_diagnostic_logger ([&] (QString const& line) {
      check (line.contains (QStringLiteral ("request_id=sync-log")),
             "control diagnostic line contains normalized request id");
      check (!line.contains (QStringLiteral ("Authorization"))
                 && !line.contains (QStringLiteral ("password")),
             "control diagnostic line has no credentials");
    });
  control.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      ++dispatch_count;
      JtdxWebControl::Dispatch prepared;
      check (control.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "diagnostic control request prepares");
      check (control.begin_dispatch (prepared), "diagnostic control request begins");
      check (control.feedback_frequency (prepared.request_id, prepared.server_epoch,
                                         prepared.expected_generation + 1, prepared.frequency_hz, 2),
             "diagnostic control request completes by readback");
    });
  JtdxWebControl::Request request;
  request.request_id = QStringLiteral (" sync-log ");
  request.operation = JtdxWebControl::Operation::Frequency;
  request.server_epoch = control.server_epoch ();
  request.state_revision = 1;
  request.frequency_hz = 14075000;
  check (control.submit (request).status == JtdxWebControl::Status::Completed,
         "control behavior is unchanged with diagnostic logging");
  check (control.submit (request).status == JtdxWebControl::Status::Completed && dispatch_count == 1,
         "duplicate request logs without duplicate execution");

  JtdxWebControl write_failure {100};
  write_failure.bind_server_epoch (write_failure.server_epoch ());
  write_failure.set_observed_state (safe_state ());
  QFile not_directory {temporary.filePath (QStringLiteral ("not-a-directory"))};
  check (not_directory.open (QIODevice::WriteOnly), "write failure fixture file is created");
  not_directory.close ();
  QString const failure_path = not_directory.fileName ();
  write_failure.set_diagnostic_logger ([&failure_path] (QString const& line) {
      JtdxLocalLog::append (QDir {failure_path}, QStringLiteral ("control.log"),
                            QStringLiteral ("web-control"), line);
    });
  write_failure.set_frequency_dispatcher ([&] (JtdxWebControl::Dispatch const& dispatch) {
      JtdxWebControl::Dispatch prepared;
      check (write_failure.prepare_dispatch (dispatch.request_id, dispatch.server_epoch, &prepared),
             "write-failure request prepares");
      check (write_failure.begin_dispatch (prepared), "write-failure request begins");
      check (write_failure.feedback_frequency (prepared.request_id, prepared.server_epoch,
                                               prepared.expected_generation + 1, prepared.frequency_hz, 2),
             "write-failure request completes by readback");
    });
  request.request_id = QStringLiteral ("write-failure");
  request.server_epoch = write_failure.server_epoch ();
  check (write_failure.submit (request).status == JtdxWebControl::Status::Completed,
         "log write failure does not alter control result");
  return 0;
}
