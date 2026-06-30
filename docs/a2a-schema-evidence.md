# A2A schema evidence

This non-video gate validates the LP-0008 Agent Card and task lifecycle envelope shape.

- Script: `scripts/run_a2a_schema_evidence.py`
- Latest summary: `docs/a2a-schema-summary-latest.json`
- Protocol: `a2a`
- Protocol version: `1.0`
- Envelope type: `a2a.task.request`
- Lifecycle: `queued -> transport_sent -> working -> completed`

Strict boundary: this gate validates Agent Card and task lifecycle schema shape. Live Logos Messaging transport is separately proven by `scripts/run_lp0008_deep_verify.py` and `scripts/run_paid_a2a_live_evidence.py`; live LEZ payment binding remains separately blocked by rc3 wallet private-send proof panic.
