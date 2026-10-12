#include "ptt_start_policy.hpp"
#include "ptt_tx_payload.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
  void require (bool condition, char const * message)
  {
    if (!condition)
      {
        std::cerr << message << '\n';
        std::exit (EXIT_FAILURE);
      }
  }
}

int main ()
{
  using Observation = PttStartPolicy::Observation;
  PttStartPolicy policy;

  PttTxPayload::Messages messages {{
    "TX1", "TX2", "TX3", "TX4", "TX5 ^", "TX6", "GENERATED", "FREE TEXT ^"
  }};
  for (int tx_index = 1; tx_index <= 8; ++tx_index)
    {
      auto const selected = PttTxPayload::select (tx_index, messages);
      auto const expected = tx_index == 5 ? "TX5"
        : tx_index == 8 ? "FREE TEXT" : messages[static_cast<std::size_t> (tx_index - 1)].c_str ();
      require (selected == expected, "TX1-TX6/generated/free-text selection matches ntx 1-8");
    }
  require (PttTxPayload::select (0, messages).empty ()
             && PttTxPayload::select (9, messages).empty (),
           "invalid TX selector has no payload");
  messages[4] = "CQ ^ DX ^ TEST";
  messages[7] = "RR73 ^ EXTRA";
  require (PttTxPayload::select (5, messages) == "CQ DX TEST"
             && PttTxPayload::select (8, messages) == "RR73 EXTRA",
           "TX5/free-text combo decorations are removed before identity and encoding");
  auto const canonical = PttTxPayload::identity ("CQ DX");
  require (canonical.size () == 37
             && canonical == PttTxPayload::identity ("CQ DX   "),
           "payload identity follows ba2msg fixed-width space padding");
  auto const long_payload = std::string (37, 'A') + "Xignored";
  require (PttTxPayload::identity (long_payload)
             == PttTxPayload::identity (std::string (37, 'A') + "Yignored"),
           "payload identity ignores bytes beyond the 37-byte encoder boundary");
  require (PttTxPayload::identity ("A B") != PttTxPayload::identity ("a B"),
           "payload identity preserves bytes that can affect the encoder input");

  auto unselected_edit = messages;
  unselected_edit[0] = "EDITED TX1";
  require (PttTxPayload::identity (PttTxPayload::select (2, messages))
             == PttTxPayload::identity (PttTxPayload::select (2, unselected_edit)),
           "editing an unselected TX message does not change active payload identity");
  auto stable_generated = messages;
  stable_generated[0] = "AUTO-GENERATED UNSELECTED";
  require (PttTxPayload::identity (PttTxPayload::select (7, messages))
             == PttTxPayload::identity (PttTxPayload::select (7, stable_generated)),
           "stable selected automatic message is not invalidated by unrelated regeneration");

  require (!PttStartPolicy::ready_for_request (true, true, false, false, true, 5),
           "FTX-1 CAT must reject unknown PTT readback");
  require (!PttStartPolicy::ready_for_request (true, true, true, false, false, 5),
           "an online/known PTT snapshot from the previous generation is not ready");
  require (!PttStartPolicy::ready_for_request (true, true, true, true, true, 5),
           "PTT-on is not a ready state for a new request");
  require (!PttStartPolicy::ready_for_request (true, true, true, false, true, 0),
           "known metadata without an actual readback serial is not ready");
  require (PttStartPolicy::ready_for_request (true, true, true, false, true, 5),
           "current-generation actual PTT-off readback is ready");
  require (PttStartPolicy::ready_for_request (false, false, false, true, false),
           "non-FTX legacy backends retain their existing request admission behavior");
  require (PttStartPolicy::may_start_request (true, false),
           "an active slot without pending tune cancellation can start");
  require (!PttStartPolicy::may_start_request (true, true),
           "tune stop closes request admission during the delayed hardware shutdown window");
  require (PttStartPolicy::should_cancel_pending_audio (false, true, false),
           "EnableTx off cancels a pending, not-yet-started audio request");
  require (!PttStartPolicy::should_cancel_pending_audio (false, false, false),
           "EnableTx off does not cancel a completed audio start");
  require (!PttStartPolicy::should_cancel_pending_audio (false, true, true),
           "independent Tune is not cancelled by EnableTx off");
  require (PttStartPolicy::should_cancel_request_at_slot_end (false, false, true),
           "gui slot end cancels a pending request once TX time ends");
  require (!PttStartPolicy::should_cancel_request_at_slot_end (true, false, true),
           "an active TX slot keeps its pending PTT/audio request");
  require (!PttStartPolicy::should_cancel_request_at_slot_end (false, true, true),
           "Tune keeps its independent request through normal TX-slot boundaries");
  require (!PttStartPolicy::should_cancel_request_at_slot_end (false, false, false),
           "slot end does not touch an already-consumed or absent request");
  require (!PttStartPolicy::fresh_auto_decision (8, 8, true, false, false),
           "the rejected decode batch cannot count as a fresh AutoSeq decision");
  require (!PttStartPolicy::fresh_auto_decision (8, 9, false, false, false),
           "AutoSeq disabled cannot clear a readiness refusal");
  require (!PttStartPolicy::fresh_auto_decision (8, 9, true, true, false),
           "manual decode cannot clear a readiness refusal");
  require (!PttStartPolicy::fresh_auto_decision (8, 9, true, false, true),
           "disk decode cannot clear a readiness refusal");
  require (PttStartPolicy::fresh_auto_decision (8, 9, true, false, false),
           "a later live AutoSeq batch is a fresh candidate decision");

  PttStartPolicy::RequestContext requested;
  requested.logical_target = "K1ABC";
  requested.band = "20m";
  requested.mode = "JT9+JT65";
  requested.tx_mode = "JT65";
  requested.dial_frequency = 14074000;
  requested.tx_audio_frequency = 1250;
  requested.tx_index = 2;
  requested.qso_phase = 4;
  requested.tx_payload_identity = PttTxPayload::identity ("TX2 original");
  auto current = requested;
  require (PttStartPolicy::same_request_context (requested, current),
           "an unchanged DX/QSO target matches its pending PTT request");
  current.logical_target = "K2XYZ";
  require (!PttStartPolicy::same_request_context (requested, current),
           "DX target takeover invalidates the pending request");
  current = requested;
  current.dial_frequency += 1000;
  require (!PttStartPolicy::same_request_context (requested, current),
           "same-band dial changes invalidate the pending request");
  current = requested;
  current.band = "40m";
  require (!PttStartPolicy::same_request_context (requested, current),
           "band changes invalidate the pending request");
  current = requested;
  current.mode = "FT8";
  require (!PttStartPolicy::same_request_context (requested, current),
           "mode changes invalidate the pending request");
  current = requested;
  current.tx_mode = "JT9";
  require (!PttStartPolicy::same_request_context (requested, current),
           "mixed-mode TX selection changes invalidate the pending request");
  current = requested;
  current.tx_index = 3;
  require (!PttStartPolicy::same_request_context (requested, current),
           "TX step changes invalidate the pending request");
  current = requested;
  current.qso_phase = 5;
  require (!PttStartPolicy::same_request_context (requested, current),
           "QSO phase changes invalidate the pending request");
  current = requested;
  current.tx_payload_identity = PttTxPayload::identity ("TX2 edited");
  require (!PttStartPolicy::same_request_context (requested, current),
           "editing the currently selected message invalidates the pending request");
  auto const changedPayloadToken = policy.begin (13, 1, true);
  require (policy.observe (changedPayloadToken, 13, true, true, true, 2)
             == Observation::audio_armed,
           "a fresh actual PTT-on readback arms the message-specific request");
  if (!PttStartPolicy::same_request_context (requested, current)) policy.cancel ();
  require (!policy.consume_audio (changedPayloadToken, 13, true, true, true, 2),
           "a selected-message context change cancels the armed token before audio consumption");

  auto generated_context = requested;
  generated_context.tx_index = 7;
  generated_context.tx_payload_identity = PttTxPayload::identity (
    PttTxPayload::select (7, messages));
  auto generated_context_after_unselected_edit = generated_context;
  generated_context_after_unselected_edit.tx_payload_identity = PttTxPayload::identity (
    PttTxPayload::select (7, stable_generated));
  require (PttStartPolicy::same_request_context (
             generated_context, generated_context_after_unselected_edit),
           "stable selected genMsg does not self-cancel when only an unselected field changes");
  auto tx2_context = requested;
  tx2_context.tx_payload_identity = PttTxPayload::identity (
    PttTxPayload::select (2, messages));
  auto tx2_context_after_unselected_edit = tx2_context;
  tx2_context_after_unselected_edit.tx_payload_identity = PttTxPayload::identity (
    PttTxPayload::select (2, unselected_edit));
  auto const unselectedEditToken = policy.begin (14, 1, true);
  require (policy.observe (unselectedEditToken, 14, true, true, true, 2)
             == Observation::audio_armed,
           "actual PTT-on arms a token with the selected TX2 payload");
  if (!PttStartPolicy::same_request_context (
        tx2_context, tx2_context_after_unselected_edit)) policy.cancel ();
  require (policy.consume_audio (unselectedEditToken, 14, true, true, true, 2),
           "an unselected-message edit keeps the armed token consumable");
  require (!policy.consume_audio (unselectedEditToken, 14, true, true, true, 2),
           "the unselected-edit token still consumes audio at most once");
  current = requested;
  current.logical_target = PttStartPolicy::logical_target ("K1ABC", true);
  requested.logical_target.clear ();
  require (PttStartPolicy::same_request_context (requested, current),
           "a CQ request has no DX identity and survives stale DX field cleanup");
  requested.tune = true;
  requested.tx_payload_identity = "ignored Tune identity";
  current = requested;
  current.logical_target = "K2XYZ";
  current.dial_frequency = 7000000;
  current.tx_index = 6;
  current.tx_payload_identity = "unrelated Tune payload";
  require (PttStartPolicy::same_request_context (requested, current),
           "Tune keeps an independent request identity across DX/QSO edits");
  current.tune = false;
  require (!PttStartPolicy::same_request_context (requested, current),
           "leaving Tune invalidates a pending Tune request");

  require (PttStartPolicy::may_rearm_initial_owner (true, true, true, true, false, false),
           "fresh decision may re-arm the same software-owned initial RCQ/RFIN candidate");
  require (!PttStartPolicy::may_rearm_initial_owner (true, false, true, true, false, false),
           "fresh decision cannot re-arm after the initial owner was released");
  require (!PttStartPolicy::may_rearm_initial_owner (true, true, false, true, false, false),
           "fresh decision cannot migrate the disabled owner to another DX call");
  require (!PttStartPolicy::may_rearm_initial_owner (true, true, true, false, false, false),
           "fresh decision cannot re-arm an owner already inside the QSO");
  require (!PttStartPolicy::may_rearm_initial_owner (true, true, true, true, true, false),
           "recovery tickets keep their existing independent resume path");
  require (!PttStartPolicy::may_rearm_initial_owner (true, true, true, true, false, true),
           "operator-held ownership is never auto-rearmed");

  auto const old = policy.begin (7, 90, true);
  require (policy.observe (old, 8, true, true, true, 91) == Observation::stale,
           "old-generation PTT-on is ignored");
  require (!policy.consume_audio (old, 8, true, true, true, 91),
           "old-generation timer cannot consume the new session");

  require (policy.observe (old, 7, true, true, false, 90)
             == Observation::waiting_for_readback,
           "a readback at the request baseline cannot satisfy the pending request");
  require (policy.observe (old, 7, true, true, false, 91)
             == Observation::waiting_for_ptt_on,
           "first fresh actual PTT-off after the request waits; it does not cancel/retry");
  require (policy.observe (old, 7, true, true, true, 91)
             == Observation::duplicate_readback,
           "the same readback cannot be reinterpreted as a PTT-on edge");
  require (policy.observe (old, 7, true, true, true, 92) == Observation::audio_armed,
           "a fresh current-generation actual PTT-on arms audio");
  require (policy.observe (old, 7, true, true, true, 92)
             == Observation::duplicate_readback,
           "repeated delivery of one actual readback does not arm audio twice");
  require (policy.observe (old, 7, true, false, true, 93)
             == Observation::waiting_for_readback,
           "unknown readback disarms the pending timer");
  require (!policy.consume_audio (old, 7, true, true, true, 93),
           "timer cannot use stale armed state after readback invalidation");
  require (policy.observe (old, 7, true, true, true, 94) == Observation::audio_armed,
           "later fresh actual PTT-on can re-arm without another PTT request");
  require (policy.consume_audio (old, 7, true, true, true, 94),
           "current request consumes a valid timer exactly once");
  require (!policy.consume_audio (old, 7, true, true, true, 94),
           "a timer cannot start audio twice");
  require (policy.observe (old, 7, true, true, true, 95)
             == Observation::audio_already_started,
           "later PTT polls cannot arm a second timer after audio starts");
  require (policy.observe (old, 7, true, true, false, 96)
             == Observation::ptt_went_off_after_confirmation,
           "actual PTT-off after actual PTT-on remains a safe-halt condition");

  // A replacement/new rig object restarts its local readback counter. begin()
  // must also reset policy observation state without relying on cancel().
  auto const nextGeneration = policy.begin (9, 0, true);
  require (policy.observe (nextGeneration, 9, true, true, true, 1)
             == Observation::audio_armed,
           "a new generation's first serial can arm after an older generation reached 100");

  auto const cancelled = policy.begin (11, 1, true);
  policy.cancel ();
  require (!policy.consume_audio (cancelled, 11, true, true, true, 2),
           "cancelled request rejects a queued timer");
  require (policy.observe (cancelled, 11, true, true, true, 2) == Observation::stale,
           "cancelled request rejects late PTT-on");

  auto const tuneRequest = policy.begin (12, 0, false);
  policy.cancel ();
  require (!policy.consume_audio (tuneRequest, 12, true, true, true),
           "tune-off cancellation rejects a delayed audio timer");
  require (!PttStartPolicy::may_start_request (true, true),
           "no replacement request can start before the tune-stop sequence finishes");
  return EXIT_SUCCESS;
}
