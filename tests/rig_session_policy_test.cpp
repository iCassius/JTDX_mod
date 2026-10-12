#include "RigSessionPolicy.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
  void require (bool condition, char const * description)
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
  using RigSessionPolicy::FailureAction;
  using RigSessionPolicy::Purpose;

  require (FailureAction::forward_to_runtime
             == RigSessionPolicy::failure_action (7, 7, Purpose::runtime),
           "current runtime session failures are forwarded to MainWindow");
  require (FailureAction::report_to_configuration
             == RigSessionPolicy::failure_action (7, 7, Purpose::configuration_test),
           "current configuration-test failures remain local");
  require (FailureAction::ignore_stale
             == RigSessionPolicy::failure_action (6, 7, Purpose::runtime),
           "stale failures are ignored regardless of old session purpose");
  require (Purpose::runtime
             == RigSessionPolicy::purpose_after_reuse (Purpose::configuration_test,
                                                        Purpose::runtime),
           "runtime callers promote an active configuration-test session");
  require (Purpose::runtime
             == RigSessionPolicy::purpose_after_reuse (Purpose::runtime,
                                                        Purpose::configuration_test),
           "configuration-test callers preserve an active runtime session");

  require (RigSessionPolicy::callback_is_current (7, 7, true),
           "current-generation active callbacks are accepted");
  require (!RigSessionPolicy::callback_is_current (6, 7, true),
           "old-generation callbacks are ignored even after a new session opens");
  require (!RigSessionPolicy::callback_is_current (7, 7, false),
           "callbacks queued during close are ignored while no rig is active");
  require (RigSessionPolicy::callback_is_current (8, 8, true),
           "callbacks from a newly active generation are accepted");

#ifdef JTDX_SOURCE_DIR
  std::ifstream config_file (std::string {JTDX_SOURCE_DIR} + "/Configuration.cpp");
  std::string config_source {std::istreambuf_iterator<char> {config_file},
                              std::istreambuf_iterator<char> {}};
  config_source.erase (std::remove (config_source.begin (), config_source.end (), '\r'),
                       config_source.end ());
  require (static_cast<bool> (config_file) || !config_source.empty (),
           "Configuration.cpp is available for callback wiring contract checks");
  auto count_occurrences = [] (std::string const& text, std::string const& needle) {
    std::size_t count {0};
    std::size_t position {0};
    while ((position = text.find (needle, position)) != std::string::npos)
      {
        ++count;
        position += needle.size ();
      }
    return count;
  };
  auto const guard = std::string {
    "RigSessionPolicy::callback_is_current (\n                    generation, rig_generation_, rig_active_)"};
  require (count_occurrences (config_source, guard) >= 3,
           "resolution, TCI frame-write and TCI mod-active callbacks all use the generation/active guard");
  require (config_source.find ("[this, generation] (int resolution)") != std::string::npos
             && config_source.find ("[this, generation] (qint64 count)") != std::string::npos
             && config_source.find ("[this, generation] (bool on)") != std::string::npos,
           "each queued side callback captures the generation that created its rig");
  auto const close_start = config_source.find ("void Configuration::impl::close_rig");
  auto const generation_advance = config_source.find ("++rig_generation_;", close_start);
  auto const close_resolution_reset = config_source.find ("rig_resolution_ = 0;", generation_advance);
  require (close_start != std::string::npos && generation_advance != std::string::npos
             && close_resolution_reset != std::string::npos,
           "closing a rig advances generation and resets resolution to its default");
  auto const create_start = config_source.find ("bool Configuration::impl::open_rig");
  auto const active_assignment = config_source.find ("rig_active_ = true;", create_start);
  auto const create_resolution_reset = config_source.find ("rig_resolution_ = 0;", active_assignment);
  require (create_start != std::string::npos && active_assignment != std::string::npos
             && create_resolution_reset != std::string::npos,
           "a newly created rig starts with default resolution until its own callback arrives");
#endif
  return 0;
}
