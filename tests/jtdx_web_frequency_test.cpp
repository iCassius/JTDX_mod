#include "JtdxWebFrequency.hpp"

#include "Bands.hpp"

#include <iostream>

namespace
{
  void check (bool condition, char const * message)
  {
    if (!condition)
      {
        std::cerr << "FAIL: " << message << '\n';
        std::exit (1);
      }
  }
}

int main ()
{
  using JtdxWebFrequency::parse_hz;
  using JtdxWebFrequency::parse_and_validate_hz;

  auto parsed = parse_hz (QStringLiteral ("14074000"));
  check (parsed.valid && parsed.frequency_hz == 14074000u, "decimal Hz parses exactly");
  check (!parse_hz (QStringLiteral (" 14074000")).valid, "leading whitespace rejected");
  check (!parse_hz (QStringLiteral ("14074000 ")).valid, "trailing whitespace rejected");
  check (!parse_hz (QStringLiteral ("+14074000")).valid, "sign rejected");
  check (!parse_hz (QStringLiteral ("-1")).valid, "negative rejected");
  check (!parse_hz (QStringLiteral ("14074000.0")).valid, "fractional Hz rejected");
  check (!parse_hz (QStringLiteral ("1e7")).valid, "exponent rejected");
  check (!parse_hz (QStringLiteral ("NaN")).valid, "NaN rejected");
  check (!parse_hz (QStringLiteral ("0")).valid, "zero rejected");
  check (!parse_hz (QStringLiteral ("9223372036854775808")).valid, "signed overflow rejected");
  check (!parse_hz (QStringLiteral ("14074000０")).valid, "non-ASCII digit rejected");

  Bands bands;
  auto in_band = parse_and_validate_hz (QStringLiteral ("14074000"), bands);
  check (in_band.valid && in_band.frequency_hz == 14074000u, "known amateur band accepted");
  check (!parse_and_validate_hz (QStringLiteral ("1000000"), bands).valid,
         "out-of-band frequency rejected");
  check (parse_and_validate_hz (QStringLiteral ("1800000"), bands).valid,
         "band lower boundary accepted");
  check (parse_and_validate_hz (QStringLiteral ("2000000"), bands).valid,
         "band upper boundary accepted");

  std::cout << "Web frequency policy checks passed\n";
}
