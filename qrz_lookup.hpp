#ifndef QRZ_LOOKUP_HPP__
#define QRZ_LOOKUP_HPP__

#include <QRegularExpression>
#include <QUrl>

namespace QRZLookup
{
  inline QString normalizeCall (QString value)
  {
    value = value.trimmed ().toUpper ();
    while (!value.isEmpty () && QStringLiteral ("<>[](){}.,;:!?\"").contains (value.front ()))
      value.remove (0, 1);
    while (!value.isEmpty () && QStringLiteral ("<>[](){}.,;:!?\"").contains (value.back ()))
      value.chop (1);

    if (value == QStringLiteral ("CQ") || value == QStringLiteral ("DE")
        || value == QStringLiteral ("QRZ") || value == QStringLiteral ("RRR")
        || value == QStringLiteral ("RR73") || value == QStringLiteral ("73"))
      return {};

    // FT8 calls include prefixes such as 3DA0RU and 4U1UN, as well as
    // portable/region qualifiers on either side of the base call.  Keep the
    // accepted alphabet deliberately narrow and require a callsign-shaped
    // base (one to two letters, a digit, then an optional suffix).
    static QRegularExpression const baseCallExpression {
      QStringLiteral (R"(^[2-9]?[A-Z]{1,2}[0-9]{1,4}[A-Z0-9]{0,7}$)")
    };
    static QRegularExpression const qualifierExpression {
      QStringLiteral (R"(^[A-Z0-9]{1,4}$)")
    };
    auto const parts = value.split ('/');
    auto const isBaseCall = [&] (QString const& part) {
      return baseCallExpression.match (part).hasMatch ();
    };
    auto const isQualifier = [&] (QString const& part) {
      return qualifierExpression.match (part).hasMatch ();
    };
    bool valid = false;
    if (parts.size () == 1) {
      valid = isBaseCall (parts.front ());
    } else if (parts.size () == 2) {
      valid = (isBaseCall (parts.at (0)) && isQualifier (parts.at (1)))
           || (isQualifier (parts.at (0)) && isBaseCall (parts.at (1)));
    }
    if (!valid) return {};
    return value;
  }

  inline QUrl urlForCall (QString const& value)
  {
    auto const call = normalizeCall (value);
    if (call.isEmpty ()) return {};
    return QUrl {QStringLiteral ("https://www.qrz.com/db/")
                 + QString::fromLatin1 (QUrl::toPercentEncoding (call, "", "/"))};
  }
}

#endif
