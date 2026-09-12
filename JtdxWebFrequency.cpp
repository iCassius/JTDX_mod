#include "JtdxWebFrequency.hpp"

#include <Bands.hpp>

#include <limits>
#include <utility>

namespace
{
  JtdxWebFrequency::Result invalid (QString reason)
  {
    JtdxWebFrequency::Result result;
    result.reason = std::move (reason);
    return result;
  }
}

JtdxWebFrequency::Result JtdxWebFrequency::parse_hz (QString const& input)
{
  if (input.isEmpty ()) return invalid (QStringLiteral ("empty"));
  if (input.size () > 19) return invalid (QStringLiteral ("out_of_range"));

  // Keep the value representable by JtdxWebControl::Request::frequency_hz,
  // which is signed to make invalid negative values unambiguous.
  constexpr quint64 max_frequency = static_cast<quint64> (std::numeric_limits<qint64>::max ());
  quint64 value {0};
  for (QChar const character : input)
    {
      uint const digit = character.unicode () >= '0' && character.unicode () <= '9'
        ? character.unicode () - '0' : 10;
      if (digit > 9) return invalid (QStringLiteral ("not_decimal_hz"));
      if (value > (max_frequency - digit) / 10u)
        return invalid (QStringLiteral ("out_of_range"));
      value = value * 10u + digit;
    }
  if (value == 0) return invalid (QStringLiteral ("out_of_range"));

  Result result;
  result.valid = true;
  result.frequency_hz = static_cast<Frequency> (value);
  return result;
}

JtdxWebFrequency::Result JtdxWebFrequency::validate_hz (Frequency frequency_hz,
                                                         Bands const& bands)
{
  constexpr Frequency max_frequency = static_cast<Frequency> (std::numeric_limits<qint64>::max ());
  if (frequency_hz == 0 || frequency_hz > max_frequency)
    return invalid (QStringLiteral ("out_of_range"));
  if (bands.find (frequency_hz).isEmpty ())
    return invalid (QStringLiteral ("out_of_band"));

  Result result;
  result.valid = true;
  result.frequency_hz = frequency_hz;
  return result;
}

JtdxWebFrequency::Result JtdxWebFrequency::parse_and_validate_hz (QString const& input,
                                                                    Bands const& bands)
{
  Result parsed = parse_hz (input);
  if (!parsed.valid) return parsed;
  return validate_hz (parsed.frequency_hz, bands);
}
