#include "JtdxWebDx.hpp"

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
}

int main ()
{
  auto valid = JtdxWebDx::normalize (QStringLiteral (" ja2lcp "), QStringLiteral ("pm95"));
  check (valid.valid && valid.call == QStringLiteral ("JA2LCP")
             && valid.grid == QStringLiteral ("PM95"), "valid call and grid normalize");
  check (JtdxWebDx::normalize (QStringLiteral ("K1ABC/P"), QStringLiteral ("FN31" )).valid,
         "legal compound call remains selectable");
  for (QString const& value : {QStringLiteral (""), QStringLiteral ("A"), QStringLiteral ("CQ"),
                               QStringLiteral ("RR73"), QStringLiteral ("JA2 LCP"), QStringLiteral ("!BAD")})
    check (!JtdxWebDx::normalize (value).valid, "invalid or protocol call is rejected");
  for (QString const& value : {QStringLiteral ("PM9"), QStringLiteral ("ZZ99"),
                               QStringLiteral ("PM95ZZ"), QStringLiteral ("PM95AA00ZZ")})
    check (!JtdxWebDx::normalize (QStringLiteral ("JA2LCP"), value).valid,
           "invalid grid length or alphabet is rejected");
  check (JtdxWebDx::normalize (QStringLiteral ("JA2LCP"), QStringLiteral ("PM95AA")).valid,
         "six-character grid is accepted");
  std::cout << "Web DX validation checks passed\n";
  return 0;
}
