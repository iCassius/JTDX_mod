#include "JtdxWebRadioAdapter.hpp"

bool JtdxWebRadioAdapter::dispatch (JtdxWebControl& control,
                                    JtdxWebControl::Dispatch const& incoming,
                                    Apply const& apply,
                                    PublishAndRead const& publish_and_read)
{
  if (incoming.operation != JtdxWebControl::Operation::Radio || !apply || !publish_and_read)
    return false;

  JtdxWebControl::Dispatch prepared;
  if (!control.prepare_dispatch (incoming.request_id, incoming.server_epoch, &prepared)
      || !control.begin_dispatch (prepared))
    return false;

  QString failure_reason;
  try
    {
      if (!apply (prepared, &failure_reason))
        {
          control.fail (prepared.request_id, prepared.server_epoch,
                        failure_reason.isEmpty () ? QStringLiteral ("radio_action_failed")
                                                  : std::move (failure_reason));
          return false;
        }

      JtdxWebControl::ObservedState const observed = publish_and_read ();
      return control.feedback_radio (prepared.request_id, prepared.server_epoch,
                                     observed.state_revision, observed);
    }
  catch (...)
    {
      control.fail (prepared.request_id, prepared.server_epoch,
                    QStringLiteral ("radio_dispatch_exception"));
      return false;
  }
}

bool JtdxWebRadioAdapter::observe (JtdxWebControl& control, QString const& request_id,
                                   QString const& server_epoch,
                                   JtdxWebControl::ObservedState const& observed)
{
  return control.feedback_radio (request_id, server_epoch, observed.state_revision, observed);
}
