#ifndef PTT_ADMISSION_POLICY_HPP__
#define PTT_ADMISSION_POLICY_HPP__

#include <atomic>
#include <cstdint>
#include <limits>

// Production grant/revoke publication is single-writer on the Configuration
// thread; worker threads only perform acquire reads through allows/current.

class PttAdmissionBlock final
{
public:
  using Token = std::uint64_t;

  Token grant () noexcept
  {
    auto token = next_token_.load (std::memory_order_relaxed);
    for (;;)
      {
        if (!token || token == std::numeric_limits<Token>::max ())
          {
            exhausted_.store (true, std::memory_order_release);
            active_token_.store (0, std::memory_order_release);
            return 0;
          }
        if (next_token_.compare_exchange_weak (
              token, token + 1, std::memory_order_relaxed,
              std::memory_order_relaxed))
          {
            if (exhausted_.load (std::memory_order_acquire)) return 0;
            active_token_.store (token, std::memory_order_release);
            return token;
          }
      }
  }

  void revoke () noexcept {active_token_.store (0, std::memory_order_release);}
  Token current () const noexcept {return active_token_.load (std::memory_order_acquire);}
  bool allows (Token queued_token) const noexcept
  {
    return queued_token && queued_token == current ();
  }
  bool exhausted () const noexcept {return exhausted_.load (std::memory_order_acquire);}

private:
  std::atomic<Token> next_token_ {1};
  std::atomic<Token> active_token_ {0};
  std::atomic<bool> exhausted_ {false};
};

#endif
