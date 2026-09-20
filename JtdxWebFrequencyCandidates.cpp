#include "JtdxWebFrequencyCandidates.hpp"

#include <algorithm>

namespace JtdxWebFrequencyCandidates
{
  JtdxWebState::FrequencyCandidates build (FrequencyList_v2 const& frequencies,
                                           Bands const& bands,
                                           IARURegions::Region region,
                                           Modes::Mode mode,
                                           int limit)
  {
    JtdxWebState::FrequencyCandidates candidates;
    QHash<quint64, int> candidate_rows;
    int const bounded_limit = qBound (0, limit, JtdxWebState::hard_frequency_candidate_limit);
    for (auto const& item : frequencies.frequency_list ())
      {
        bool const region_matches = region == IARURegions::ALL
          || item.region_ == IARURegions::ALL || item.region_ == region;
        bool const mode_matches = mode == Modes::ALL
          || item.mode_ == Modes::ALL || item.mode_ == mode;
        QString const band = bands.find (item.frequency_);
        if (!region_matches || !mode_matches || band.isEmpty ()) continue;

        int const existing = candidate_rows.value (item.frequency_, -1);
        if (existing >= 0)
          {
            auto& candidate = candidates[existing];
            candidate.default_frequency = candidate.default_frequency || item.default_;
            if (item.mode_ == mode)
              candidate.mode = QString::fromLatin1 (Modes::name (item.mode_));
            if (item.region_ == region)
              candidate.region = QString::fromLatin1 (IARURegions::name (item.region_));
            continue;
          }

        JtdxWebState::FrequencyCandidate candidate;
        candidate.frequency_hz = item.frequency_;
        candidate.band = band;
        candidate.mode = QString::fromLatin1 (Modes::name (item.mode_));
        candidate.region = QString::fromLatin1 (IARURegions::name (item.region_));
        candidate.default_frequency = item.default_;
        candidate_rows.insert (item.frequency_, candidates.size ());
        candidates.append (std::move (candidate));
      }

    std::sort (candidates.begin (), candidates.end (), [] (JtdxWebState::FrequencyCandidate const& lhs,
                                                           JtdxWebState::FrequencyCandidate const& rhs) {
        return lhs.frequency_hz < rhs.frequency_hz;
      });
    if (candidates.size () > bounded_limit)
      candidates.erase (candidates.begin () + bounded_limit, candidates.end ());
    return candidates;
  }
}
