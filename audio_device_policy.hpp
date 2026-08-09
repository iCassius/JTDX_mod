#ifndef AUDIO_DEVICE_POLICY_HPP__
#define AUDIO_DEVICE_POLICY_HPP__

namespace AudioDeviceSelectionPolicy
{
  enum class Match
  {
    ExactObject,
    UniqueName,
    AmbiguousName,
    NotFound
  };

  inline Match choose (bool exactObjectFound, unsigned sameNameCount)
  {
    if (exactObjectFound) return Match::ExactObject;
    if (sameNameCount == 1u) return Match::UniqueName;
    if (sameNameCount > 1u) return Match::AmbiguousName;
    return Match::NotFound;
  }
}

#endif
