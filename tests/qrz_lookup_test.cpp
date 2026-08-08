#include "qrz_lookup.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
  void expect (bool condition, char const * description)
  {
    if (!condition) {
      std::cerr << "failed: " << description << '\n';
      std::exit (1);
    }
  }
}

int main ()
{
  expect (QRZLookup::normalizeCall (" bi7kgd ") == "BI7KGD", "normalize BI7KGD");
  expect (QRZLookup::normalizeCall ("SP3SLA") == "SP3SLA", "accept SP3SLA");
  expect (QRZLookup::normalizeCall ("4U1UN") == "4U1UN", "accept 4U1UN");
  expect (QRZLookup::normalizeCall ("3DA0RU") == "3DA0RU", "accept 3DA0RU");
  expect (QRZLookup::normalizeCall ("BI7KGD/P") == "BI7KGD/P", "accept portable suffix");
  expect (QRZLookup::normalizeCall ("F/SP3SLA") == "F/SP3SLA", "accept regional prefix");
  expect (QRZLookup::normalizeCall ("<W1ABC>") == "W1ABC", "strip decoded-call punctuation");
  expect (QRZLookup::normalizeCall ("CQ").isEmpty (), "reject CQ keyword");
  expect (QRZLookup::normalizeCall ("RRR").isEmpty (), "reject RRR protocol field");
  expect (QRZLookup::normalizeCall ("RR73").isEmpty (), "reject RR73 protocol field");
  expect (QRZLookup::normalizeCall ("73").isEmpty (), "reject 73 protocol field");
  expect (QRZLookup::normalizeCall ("TEST").isEmpty (), "reject ordinary word");
  expect (QRZLookup::normalizeCall ("73ABC").isEmpty (), "reject malformed call text");
  expect (QRZLookup::urlForCall ("BI7KGD").toString () == "https://www.qrz.com/db/BI7KGD",
          "build QRZ URL");
  expect (!QRZLookup::urlForCall ("RR73").isValid (), "do not build URL for protocol field");
  return 0;
}
