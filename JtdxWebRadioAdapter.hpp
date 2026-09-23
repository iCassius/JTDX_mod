#ifndef JTDXWEBRADIOADAPTER_HPP
#define JTDXWEBRADIOADAPTER_HPP

#include "JtdxWebControl.hpp"

#include <functional>

// Coordinates exactly one production Radio dispatch with the existing desktop
// action entry and a fresh post-action MainWindow observation.
class JtdxWebRadioAdapter final
{
public:
  using Apply = std::function<bool (JtdxWebControl::Dispatch const&, QString *)>;
  using PublishAndRead = std::function<JtdxWebControl::ObservedState ()>;

  static bool dispatch (JtdxWebControl& control,
                        JtdxWebControl::Dispatch const& incoming,
                        Apply const& apply,
                        PublishAndRead const& publish_and_read);
};

#endif
