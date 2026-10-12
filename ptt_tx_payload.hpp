#ifndef PTT_TX_PAYLOAD_HPP__
#define PTT_TX_PAYLOAD_HPP__

#include <array>
#include <cstddef>
#include <string>

// Select and identify the same user message that guiUpdate() passes to
// ba2msg().  This helper deliberately knows only the TX1-TX6, generated
// message, and free-text selectors; Tune and WSPR use separate paths.
namespace PttTxPayload
{
  using Messages = std::array<std::string, 8>;

  inline std::string select (int tx_index, Messages const& messages)
  {
    if (tx_index < 1 || tx_index > static_cast<int> (messages.size ()))
      return {};

    auto text = messages[static_cast<std::size_t> (tx_index - 1)];
    if (tx_index == 5 || tx_index == 8)
      {
        std::string::size_type position = 0;
        while ((position = text.find (" ^", position)) != std::string::npos)
          text.erase (position, 2);
      }
    return text;
  }

  // ba2msg() copies exactly 37 bytes and space-pads a shorter message.  A
  // fixed-width identity therefore ignores only trailing padding and content
  // beyond the encoder's actual input boundary, while preserving all other
  // bytes exactly as encoded.
  inline std::string identity (std::string const& selected_text)
  {
    auto const length = selected_text.size () < 37 ? selected_text.size () : 37;
    std::string result (37, ' ');
    result.replace (0, length, selected_text.data (), length);
    return result;
  }
}

#endif // PTT_TX_PAYLOAD_HPP__
