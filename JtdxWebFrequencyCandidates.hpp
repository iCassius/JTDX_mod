#ifndef JTDX_WEB_FREQUENCY_CANDIDATES_HPP
#define JTDX_WEB_FREQUENCY_CANDIDATES_HPP

#include "JtdxWebState.hpp"

#include "Bands.hpp"
#include "FrequencyList.hpp"

namespace JtdxWebFrequencyCandidates
{
  JtdxWebState::FrequencyCandidates build (FrequencyList_v2 const& frequencies,
                                           Bands const& bands,
                                           IARURegions::Region region,
                                           Modes::Mode mode,
                                           int limit = JtdxWebState::default_frequency_candidate_limit);
}

#endif
