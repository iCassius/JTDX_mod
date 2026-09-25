#ifndef JTDX_WEB_ADDRESS_HPP
#define JTDX_WEB_ADDRESS_HPP

#include <QHostAddress>
#include <QList>
#include <QVector>

namespace JtdxWebAddress
{
  struct DefaultRouteCandidate
  {
    quint32 interface_index {0};
    quint32 route_metric {0};
    quint32 interface_metric {0};
    bool interface_up {false};
    bool interface_running {false};
    QList<QHostAddress> ipv4_addresses;
  };

  // Pure policy seam: the platform provider supplies IPv4 default routes and
  // active interface addresses; no socket is opened to discover a route.
  QHostAddress select_default_route_ipv4 (QVector<DefaultRouteCandidate> const& candidates);
  QHostAddress accessible_address (QHostAddress const& bind_address,
                                   QHostAddress const& default_route_address);
  QHostAddress default_route_ipv4 ();
}

#endif
