#include "JtdxWebAddress.hpp"

#include <QNetworkAddressEntry>
#include <QNetworkInterface>

#include <algorithm>
#include <limits>
#include <utility>

#ifdef Q_OS_WIN
#  include <winsock2.h>
#  include <windows.h>
#  include <ws2ipdef.h>
#  include <iphlpapi.h>
#  include <netioapi.h>
#endif

namespace
{
bool is_routable_ipv4 (QHostAddress const& address)
{
  if (address.protocol () != QAbstractSocket::IPv4Protocol || address.isNull ()
      || address.isLoopback () || address.isMulticast ()) return false;

  quint32 const value = address.toIPv4Address ();
  return value != 0
      && value != std::numeric_limits<quint32>::max ()
      && (value & 0xffff0000u) != 0xa9fe0000u; // IPv4 link-local (169.254/16)
}
}

namespace JtdxWebAddress
{
QHostAddress select_default_route_ipv4 (QVector<DefaultRouteCandidate> const& candidates)
{
  QVector<DefaultRouteCandidate const *> ordered;
  ordered.reserve (candidates.size ());
  for (auto const& candidate : candidates)
    {
      if (candidate.interface_up && candidate.interface_running)
        ordered.push_back (&candidate);
    }

  std::stable_sort (ordered.begin (), ordered.end (), [] (DefaultRouteCandidate const * lhs,
                                                           DefaultRouteCandidate const * rhs) {
    auto const lhs_metric = static_cast<quint64> (lhs->route_metric) + lhs->interface_metric;
    auto const rhs_metric = static_cast<quint64> (rhs->route_metric) + rhs->interface_metric;
    if (lhs_metric != rhs_metric) return lhs_metric < rhs_metric;
    return lhs->interface_index < rhs->interface_index;
  });

  for (auto const * candidate : ordered)
    {
      for (auto const& address : candidate->ipv4_addresses)
        if (is_routable_ipv4 (address)) return address;
    }
  return {};
}

QHostAddress accessible_address (QHostAddress const& bind_address,
                                 QHostAddress const& default_route_address)
{
  if (bind_address == QHostAddress::AnyIPv4)
    return is_routable_ipv4 (default_route_address) ? default_route_address
                                                     : QHostAddress {QHostAddress::LocalHost};
  return bind_address;
}

QHostAddress default_route_ipv4 ()
{
#ifdef Q_OS_WIN
  PMIB_IPFORWARD_TABLE2 table {nullptr};
  if (GetIpForwardTable2 (AF_INET, &table) != NO_ERROR || !table)
    {
      if (table) FreeMibTable (table);
      return {};
    }

  QVector<DefaultRouteCandidate> candidates;
  candidates.reserve (static_cast<int> (table->NumEntries));
  for (ULONG i = 0; i < table->NumEntries; ++i)
    {
      auto const& route = table->Table[i];
      auto const& prefix = route.DestinationPrefix.Prefix;
      bool const is_default = route.DestinationPrefix.PrefixLength == 0
          && prefix.si_family == AF_INET
          && reinterpret_cast<sockaddr_in const *> (&prefix)->sin_addr.s_addr == INADDR_ANY;
      if (!is_default || route.InterfaceIndex == 0) continue;

      auto const iface = QNetworkInterface::interfaceFromIndex (static_cast<int> (route.InterfaceIndex));
      auto const flags = iface.flags ();
      if (!iface.isValid ()
          || !flags.testFlag (QNetworkInterface::IsUp)
          || !flags.testFlag (QNetworkInterface::IsRunning)) continue;

      MIB_IPINTERFACE_ROW ip_interface {};
      InitializeIpInterfaceEntry (&ip_interface);
      ip_interface.Family = AF_INET;
      ip_interface.InterfaceLuid = route.InterfaceLuid;
      if (GetIpInterfaceEntry (&ip_interface) != NO_ERROR) continue;

      DefaultRouteCandidate candidate;
      candidate.interface_index = route.InterfaceIndex;
      candidate.route_metric = route.Metric;
      candidate.interface_metric = ip_interface.Metric;
      candidate.interface_up = true;
      candidate.interface_running = true;
      for (auto const& entry : iface.addressEntries ())
        {
          auto const address = entry.ip ();
          if (address.protocol () == QAbstractSocket::IPv4Protocol)
            candidate.ipv4_addresses.push_back (address);
        }
      candidates.push_back (std::move (candidate));
    }

  FreeMibTable (table);
  return select_default_route_ipv4 (candidates);
#else
  // A platform without a route-table provider safely falls back to loopback
  // rather than guessing from interface enumeration order.
  return {};
#endif
}
}
