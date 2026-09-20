#include "JtdxWebFrequencyCandidates.hpp"

#include <QCoreApplication>

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
}

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
  Bands bands;
  FrequencyList_v2 frequencies {&bands};
  frequencies.frequency_list ({
    {14074000u, Modes::FT8, IARURegions::ALL, false},
    {14074000u, Modes::FT8, IARURegions::R1, true},
    {14078000u, Modes::JT9, IARURegions::ALL, true},
    {7074000u, Modes::FT8, IARURegions::R1, true},
    {7074000u, Modes::FT8, IARURegions::R2, false},
    {1000000u, Modes::FT8, IARURegions::ALL, true}
  });

  auto const region_one_ft8 = JtdxWebFrequencyCandidates::build (
      frequencies, bands, IARURegions::R1, Modes::FT8);
  check (region_one_ft8.size () == 2, "region and mode filters keep matching frequencies only");
  check (region_one_ft8.at (0).frequency_hz == 7074000u
             && region_one_ft8.at (1).frequency_hz == 14074000u,
         "candidates are sorted by exact Hz");
  check (region_one_ft8.at (1).default_frequency,
         "duplicate frequency rows merge the default marker");
  check (region_one_ft8.at (0).band == QStringLiteral ("40m")
             && region_one_ft8.at (1).band == QStringLiteral ("20m"),
         "Bands supplies the candidate band labels");

  auto const region_two_ft8 = JtdxWebFrequencyCandidates::build (
      frequencies, bands, IARURegions::R2, Modes::FT8);
  check (region_two_ft8.size () == 2, "region-specific rows and ALL rows are accepted");
  auto const all_modes = JtdxWebFrequencyCandidates::build (
      frequencies, bands, IARURegions::ALL, Modes::ALL, 1);
  check (all_modes.size () == 1 && all_modes.first ().frequency_hz == 7074000u,
         "candidate limit is bounded after sorting");
  auto const empty = JtdxWebFrequencyCandidates::build (
      frequencies, bands, IARURegions::R3, Modes::FT4);
  check (empty.isEmpty (), "unmatched mode and region produce an empty list");

  return failures == 0 ? 0 : 1;
}
