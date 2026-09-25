#include "JtdxWebAddress.hpp"

#include <QStringList>
#include <cstdio>

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

JtdxWebAddress::DefaultRouteCandidate route (quint32 index, quint32 route_metric,
                                             quint32 interface_metric, bool up,
                                             bool running, QStringList addresses)
{
  JtdxWebAddress::DefaultRouteCandidate result;
  result.interface_index = index;
  result.route_metric = route_metric;
  result.interface_metric = interface_metric;
  result.interface_up = up;
  result.interface_running = running;
  for (auto const& address : addresses) result.ipv4_addresses.push_back (QHostAddress {address});
  return result;
}
}

int main ()
{
  using JtdxWebAddress::accessible_address;
  using JtdxWebAddress::select_default_route_ipv4;
  using JtdxWebAddress::DefaultRouteCandidate;

  QVector<DefaultRouteCandidate> routes {
    route (2, 15, 20, true, true, {QStringLiteral ("10.0.0.2")}),
    route (8, 5, 10, true, true, {QStringLiteral ("192.168.40.8")}),
    route (1, 0, 0, false, true, {QStringLiteral ("172.16.0.1")})
  };
  check (select_default_route_ipv4 (routes) == QHostAddress {QStringLiteral ("192.168.40.8")},
         "lowest combined route and interface metric selects the VPN or preferred default route");

  QVector<DefaultRouteCandidate> stable_tie {
    route (9, 10, 0, true, true, {QStringLiteral ("10.9.0.1")}),
    route (4, 10, 0, true, true, {QStringLiteral ("10.4.0.1")})
  };
  check (select_default_route_ipv4 (stable_tie) == QHostAddress {QStringLiteral ("10.4.0.1")},
         "equal-cost routes use stable interface-index order instead of enumeration order");

  QVector<DefaultRouteCandidate> address_filter {
    route (2, 1, 0, true, true, {QStringLiteral ("127.0.0.1"), QStringLiteral ("169.254.1.2"),
                                  QStringLiteral ("0.0.0.0"), QStringLiteral ("192.168.4.2")})
  };
  check (select_default_route_ipv4 (address_filter) == QHostAddress {QStringLiteral ("192.168.4.2")},
         "loopback, link-local, and unspecified addresses are skipped");

  QVector<DefaultRouteCandidate> no_route {
    route (2, 1, 0, true, true, {QStringLiteral ("169.254.1.2")}),
    route (3, 1, 0, true, false, {QStringLiteral ("10.3.0.1")})
  };
  check (select_default_route_ipv4 (no_route).isNull (),
         "disconnected interfaces or candidates without usable IPv4 yield no route address");

  auto const any_ipv4 = QHostAddress {QHostAddress::AnyIPv4};
  check (accessible_address (any_ipv4, QHostAddress {QStringLiteral ("192.168.40.8")})
             == QHostAddress {QStringLiteral ("192.168.40.8")},
         "wildcard listener advertises the selected default-route IPv4, not 0.0.0.0");
  check (accessible_address (any_ipv4, {}).isLoopback (),
         "wildcard listener falls back to loopback when route discovery has no address");
  check (accessible_address (QHostAddress {QStringLiteral ("127.0.0.1")},
                             QHostAddress {QStringLiteral ("192.168.40.8")})
             == QHostAddress {QStringLiteral ("127.0.0.1")},
         "explicit loopback binding stays local-only");

  return failures ? 1 : 0;
}
