#include "MessageClient.hpp"
#include "NetworkMessage.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QThread>
#include <QUdpSocket>

#include <cstdlib>
#include <iostream>

namespace
{
  using DatagramCounts = QHash<quint32, int>;

  void expect (bool condition, char const * description)
  {
    if (!condition)
      {
        std::cerr << "failed: " << description << '\n';
        std::exit (1);
      }
  }

  void pump (int milliseconds)
  {
    QElapsedTimer timer;
    timer.start ();
    while (timer.elapsed () < milliseconds)
      {
        QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
        QThread::msleep (2);
      }
  }

  DatagramCounts drain_counts (QUdpSocket& socket)
  {
    DatagramCounts result;
    while (socket.hasPendingDatagrams ())
      {
        QByteArray datagram;
        datagram.resize (static_cast<int> (socket.pendingDatagramSize ()));
        if (socket.readDatagram (datagram.data (), datagram.size ()) < 0)
          {
            continue;
          }
        try
          {
            NetworkMessage::Reader reader {datagram};
            auto const type = static_cast<quint32> (reader.type ());
            result[type] = result.value (type) + 1;
          }
        catch (...)
          {
            // A malformed datagram is not one of the protocol messages under test.
          }
      }
    return result;
  }

  void merge_counts (DatagramCounts& target, DatagramCounts const& source)
  {
    for (auto it = source.constBegin (); it != source.constEnd (); ++it)
      {
        target[it.key ()] = target.value (it.key ()) + it.value ();
      }
  }

  void expect_protocol_set (DatagramCounts const& counts, char const * destination)
  {
    expect (counts.value (NetworkMessage::Heartbeat) >= 1, destination);
    expect (counts.value (NetworkMessage::Status) >= 1, destination);
    expect (counts.value (NetworkMessage::Decode) >= 1, destination);
    expect (counts.value (NetworkMessage::WSPRDecode) >= 1, destination);
    expect (counts.value (NetworkMessage::Clear) >= 1, destination);
    expect (counts.value (NetworkMessage::QSOLogged) >= 1, destination);
    expect (counts.value (NetworkMessage::LoggedADIF) >= 1, destination);
  }

  void send_sample_telemetry (MessageClient& client)
  {
    auto const now = QDateTime::currentDateTimeUtc ();
    client.status_update (14074000, "FT8", "W1ABC", "-10", "FT8", true, false, true,
                          1200, 1500, "BI7KGD", "OM89", "FN31", false, {}, false, true, false);
    client.decode (true, QTime {12, 34}, -10, 0.4f, 1200, "FT8", "CQ W1ABC FN31", false, false);
    client.WSPR_decode (true, QTime {12, 34}, -20, 0.2f, 14097100, 1, "W1ABC", "FN31", 30, false);
    client.clear_decodes ();
    client.qso_logged (now, "W1ABC", "FN31", 14074000, "FT8", "-10", "-08", "50W", "", "",
                       now.addSecs (-60), "BI7KGD", "BI7KGD", "OM89");
    client.logged_ADIF ("<call:5>W1ABC\n<EOR>");
  }

  void send_one_status (MessageClient& client)
  {
    client.status_update (14074000, "FT8", "W1ABC", "-10", "FT8", false, false, false,
                          0, 0, "BI7KGD", "OM89", "FN31", false, {}, false, true, true);
  }
}

int main (int argc, char * argv[])
{
  QCoreApplication application {argc, argv};

  QUdpSocket primary_capture;
  QUdpSocket secondary_capture;
  expect (primary_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind primary capture socket");
  expect (secondary_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind secondary capture socket");

  MessageClient primary {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", primary_capture.localPort ()};
  MessageClient secondary {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", secondary_capture.localPort ()};
  primary.set_mirror (&secondary);
  secondary.set_suppressed_destination ("198.51.100.1", 9);

  // The initial heartbeat is captured before telemetry and merged into the
  // final counts, so the test never relies on the 15-second heartbeat timer.
  pump (1000);
  auto primary_counts = drain_counts (primary_capture);
  auto secondary_counts = drain_counts (secondary_capture);
  expect (primary_counts.value (NetworkMessage::Heartbeat) >= 1, "primary sends initial heartbeat");
  expect (secondary_counts.value (NetworkMessage::Heartbeat) >= 1, "secondary sends initial heartbeat");

  send_sample_telemetry (primary);
  pump (100);
  merge_counts (primary_counts, drain_counts (primary_capture));
  merge_counts (secondary_counts, drain_counts (secondary_capture));
  expect_protocol_set (primary_counts, "primary receives complete protocol telemetry");
  expect_protocol_set (secondary_counts, "secondary receives complete protocol telemetry");

  // An invalid primary target must not prevent the independent secondary from
  // receiving the mirrored status.
  QUdpSocket primary_invalid_capture;
  expect (primary_invalid_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind primary-invalid capture socket");
  MessageClient invalid_primary {"udp-mirror-test", "2.2.159.2.4", "invalid-primary.invalid", 65000};
  MessageClient valid_secondary {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", primary_invalid_capture.localPort ()};
  invalid_primary.set_mirror (&valid_secondary);
  pump (300);
  drain_counts (primary_invalid_capture);
  send_one_status (invalid_primary);
  pump (100);
  expect (drain_counts (primary_invalid_capture).value (NetworkMessage::Status) == 1,
          "invalid primary does not block secondary telemetry");

  // Conversely, an invalid secondary target must not prevent the primary from
  // receiving its own status telemetry.
  QUdpSocket secondary_invalid_capture;
  expect (secondary_invalid_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind secondary-invalid capture socket");
  MessageClient valid_primary {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", secondary_invalid_capture.localPort ()};
  MessageClient invalid_secondary {"udp-mirror-test", "2.2.159.2.4", "invalid-secondary.invalid", 65001};
  valid_primary.set_mirror (&invalid_secondary);
  pump (300);
  drain_counts (secondary_invalid_capture);
  send_one_status (valid_primary);
  pump (100);
  expect (drain_counts (secondary_invalid_capture).value (NetworkMessage::Status) == 1,
          "invalid secondary does not block primary telemetry");

  // Observation is local admission and remains available when UDP is not
  // configured; this is the source consumed by the Web state model.
  MessageClient empty_target {"udp-mirror-test", "2.2.159.2.4", QString {}, 0};
  int empty_target_observations = 0;
  QObject::connect (&empty_target, &MessageClient::decode_observed,
                    [&empty_target_observations] (bool, QTime, qint32, float, quint32,
                                                   QString const&, QString const&, bool, bool,
                                                   QString const&, QString const&) {
                      ++empty_target_observations;
                    });
  empty_target.decode (true, QTime {12, 34}, -10, 0.4f, 1200, "FT8", "CQ W1ABC FN31", false, false,
                       "W1ABC", "FN31");
  expect (empty_target_observations == 1, "empty UDP target still emits local observation");

  // A disabled client produces neither structured telemetry nor a heartbeat.
  QUdpSocket disabled_capture;
  expect (disabled_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind disabled capture socket");
  MessageClient disabled {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", disabled_capture.localPort (), nullptr, false};
  int disabled_observations = 0;
  QObject::connect (&disabled, &MessageClient::decode_observed,
                    [&disabled_observations] (bool, QTime, qint32, float, quint32,
                                               QString const&, QString const&, bool, bool,
                                               QString const&, QString const&) {
                      ++disabled_observations;
                    });
  send_sample_telemetry (disabled);
  pump (100);
  expect (disabled_observations == 1, "disabled UDP client still emits local observation");
  expect (!disabled_capture.hasPendingDatagrams (), "disabled secondary sends no packets");

  // Dynamic target changes take effect without reconstructing the client.
  QUdpSocket dynamic_capture;
  expect (dynamic_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind dynamic capture socket");
  secondary.set_server_port (dynamic_capture.localPort ());
  secondary.set_server ("127.0.0.1");
  pump (100);
  send_one_status (secondary);
  pump (100);
  expect (drain_counts (dynamic_capture).value (NetworkMessage::Status) == 1,
          "secondary target changes immediately");

  // Disabling after a live heartbeat must also suppress later heartbeat timer
  // ticks.  The wait spans one full 15-second production interval.
  QUdpSocket dynamic_disable_capture;
  expect (dynamic_disable_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind dynamic-disable capture socket");
  MessageClient dynamic_disable {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", dynamic_disable_capture.localPort ()};
  pump (1000);
  expect (drain_counts (dynamic_disable_capture).value (NetworkMessage::Heartbeat) >= 1,
          "dynamic-disable client sends initial heartbeat");
  dynamic_disable.set_enabled (false);
  pump (16000);
  expect (!dynamic_disable_capture.hasPendingDatagrams (), "disabled client sends no later heartbeat");

  // Same normalized host/port receives one datagram: count actual datagrams,
  // not only distinct protocol types.
  QUdpSocket deduplicated_capture;
  expect (deduplicated_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind deduplication capture socket");
  MessageClient primary_same {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", deduplicated_capture.localPort ()};
  MessageClient secondary_same {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", deduplicated_capture.localPort ()};
  primary_same.set_mirror (&secondary_same);
  secondary_same.set_suppressed_destination ("127.0.0.1", deduplicated_capture.localPort ());
  pump (1000);
  drain_counts (deduplicated_capture);
  send_one_status (primary_same);
  pump (100);
  auto const deduplicated_counts = drain_counts (deduplicated_capture);
  expect (deduplicated_counts.value (NetworkMessage::Status) == 1,
          "same target receives exactly one status datagram");

  // Each enabled client owns its own lifecycle Close message.
  QUdpSocket close_capture;
  expect (close_capture.bind (QHostAddress {QHostAddress::LocalHost}, 0), "bind close capture socket");
  {
    MessageClient closing_client {"udp-mirror-test", "2.2.159.2.4", "127.0.0.1", close_capture.localPort ()};
    pump (1000);
    drain_counts (close_capture);
  }
  pump (100);
  expect (drain_counts (close_capture).value (NetworkMessage::Close) == 1,
          "enabled client sends lifecycle close");
  return 0;
}
