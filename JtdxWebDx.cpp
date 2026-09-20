#include "JtdxWebDx.hpp"

#include "Radio.hpp"
#include <QStringList>

namespace
{
  bool ascii_call (QString const& value)
  {
    if (value.isEmpty () || value.size () > 32) return false;
    for (QChar const character : value)
      if (!((character >= QChar {'A'} && character <= QChar {'Z'})
            || (character >= QChar {'0'} && character <= QChar {'9'})
            || character == QChar {'/'}))
        return false;
    return true;
  }

  bool valid_grid (QString const& value)
  {
    if (value.isEmpty ()) return true;
    if (value.size () != 4 && value.size () != 6 && value.size () != 8 && value.size () != 10)
      return false;
    if (value[0] < QChar {'A'} || value[0] > QChar {'R'}
        || value[1] < QChar {'A'} || value[1] > QChar {'R'}
        || !value[2].isDigit () || !value[3].isDigit ())
      return false;
    for (int index = 4; index < value.size (); ++index)
      {
        QChar const character = value[index];
        bool const square = (index / 2) % 2 == 0;
        if (square && (character < QChar {'A'} || character > QChar {'X'})) return false;
        if (!square && !character.isDigit ()) return false;
      }
    return true;
  }
}

namespace JtdxWebDx
{
  Result normalize (QString call, QString grid)
  {
    Result result;
    result.call = call.trimmed ().toUpper ();
    result.grid = grid.trimmed ().toUpper ();
    if (!ascii_call (result.call) || result.call.size () < 3)
      {
        result.reason = QStringLiteral ("invalid_call");
        return result;
      }
    static QStringList const protocol_fields {
      QStringLiteral ("CQ"), QStringLiteral ("DE"), QStringLiteral ("QRZ"),
      QStringLiteral ("RRR"), QStringLiteral ("RR73"), QStringLiteral ("73")};
    if (protocol_fields.contains (result.call) || !Radio::is_callsign (result.call)
        || (result.call.contains ('/')
            && !Radio::is_callsign (Radio::base_callsign (result.call))))
      {
        result.reason = QStringLiteral ("invalid_call");
        return result;
      }
    if (!valid_grid (result.grid))
      {
        result.reason = QStringLiteral ("invalid_grid");
        return result;
      }
    result.valid = true;
    result.reason = QStringLiteral ("valid");
    return result;
  }
}
