// Minimal production scope fixture retained for the isolated 038 regression.
// Source S0 source raw SHA256=cc5434d55d37f55b2843806594d09f908cc0cd6c3170027f596a9ce204f259de
// Copied from the S1-frozen S0-derived production_scope_038.inc excerpt.
if(m_transmitting or m_enableTx or m_tune) {
if(g_iptt==0 && PttStartPolicy::may_start_request (
         (m_bTxTime && fTR < 99.0) || m_tune, m_tuneStopPending)) {
if(!m_bTxTime and !m_tune) m_btxok=false;
}
}
if((!m_btxok && btxok0 && g_iptt==1) || m_haltTrans) { stopTx(); if(m_haltTrans) m_haltTrans=false; }
