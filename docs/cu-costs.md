# CU cost and proof-mode notes for LP-0008

This document is a reviewer-facing cost/proof boundary for the current autonomous-agent module. It is intentionally conservative: if the available Logos APIs do not expose a stable CU meter or in-process proof path, this file says so rather than inventing numbers.

## Scope

The LP-0008 module currently exercises three execution classes:

1. Logos Core module calls (`agent_module`, `storage_module`, `delivery_module`).
2. Public-testnet LEZ token transfer through the rc3 wallet FFI/external wallet path.
3. Program query/call/deploy skill surfaces, which fail closed inside the module until Logos Core exposes a stable in-process LEZ program SDK/C ABI.

## Observed public-testnet token-transfer evidence

Latest public-testnet send evidence:

- Transaction: `dcb41d4c4a579541b591194f5701eed78762e8934a11f5a48f6fff607a974c73`
- Balance movement observed by the pre-video evidence gate: 149 LEZ -> 144 LEZ
- Sequencer: `https://testnet.lez.logos.co/`
- Wallet program family: rc3 (`cf3639d8`)

The wallet CLI/FFI path reports transaction hash and balance movement but does **not** expose a stable CU-cost field in the output currently captured by this repository. Therefore, token-transfer CU cost is recorded as: **not exposed by current rc3 wallet CLI/FFI output**.

## Program operation CU costs

`program.call` and `program.deploy` are intentionally fail-closed in `src/agent_module_impl.cpp`:

- `program.call`: `live_program_call_not_available`
- `program.deploy`: `live_program_deploy_not_available`

Reason: Logos Core currently does not expose a module-safe in-process LEZ program SDK/C ABI for arbitrary program call/deploy from this module. The rc3 SPEL/lgs path exists outside the module and is documented in `docs/upstream/program-live-api.md`, but it is not wired into the agent module as a safe runtime dependency.

Because program operations are not submitted by this module, their module-level CU costs are: **not applicable / no submitted transaction**.

## RISC0_DEV_MODE=0 boundary

The generic `demo.sh` and the Logos Core integration gates verify module behavior and live storage/delivery transport. They do not claim to generate RISC0 proofs for a custom LP-0008 program.

If a final submission requires proof-mode video for a LEZ program operation, that must be recorded through the rc3 SPEL/lgs program path, or documented as upstream-blocked for this agent-module runtime. Until then, proof-mode status for `program.call`/`program.deploy` is: **not proven by this module**.

## Current cost table

| Operation | Submission path | CU/proof status | Evidence |
|---|---|---|---|
| LEZ token send | rc3 wallet FFI/external wallet path | Public-testnet tx proven; CU field not exposed by current wallet output | tx `dcb41d4c4a579541b591194f5701eed78762e8934a11f5a48f6fff607a974c73` |
| A2A task envelope transport | `agent.task` -> `messaging.send` -> `delivery_module` | Live Logos Messaging transport proven; no LEZ execution CU | `scripts/run_lp0008_deep_verify.py`, `transport.result.mode == live` |
| Storage upload/download/list | `storage_module` client path | Live module integration proven; no LEZ CU field exposed | `scripts/run_lp0008_deep_verify.py` |
| Program call | `program.call` | Fail-closed; no submitted tx; CU not applicable | `docs/upstream/program-live-api.md` |
| Program deploy | `program.deploy` | Fail-closed; no submitted tx; CU not applicable | `docs/upstream/program-live-api.md` |

## Strict acceptance note

This file satisfies documentation honesty, not full strict proof-mode acceptance. The remaining strict gap is to produce a reviewer-facing proof-mode story if LP-0008 evaluators require `RISC0_DEV_MODE=0` for a custom program operation rather than accepting the autonomous-agent module boundaries above.
