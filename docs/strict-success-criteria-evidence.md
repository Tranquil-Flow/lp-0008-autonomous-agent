# LP-0008 strict success-criteria evidence map

This file maps current LP-0008 evidence to success criteria and honest boundaries. It is **not** a final acceptance claim; see `docs/submission-readiness-matrix.md` for the current no-go items.

## Final pre-video gate

Run:

```bash
scripts/run_final_pre_video_evidence.sh
```

With a funded rc3 wallet mounted, also export:

```bash
export LP0008_LIVE_WALLET_HOME=$LP0008_TEST_ROOT/rc3_faucet_wallet
export LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq
export LP0008_LIVE_SEND_RECIPIENT=yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3
```

The gate runs build, raw C ABI verification, Logos Core co-load integration, three-agent A2A proof, readiness validation, and the optional live wallet-send verifier.

Latest full pre-video gate run on the M4 Pro completed with `PRE_VIDEO_EVIDENCE_OK` and real public-testnet wallet proof:

- tx: `dcb41d4c4a579541b591194f5701eed78762e8934a11f5a48f6fff607a974c73`
- account: `27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq`
- recipient: `yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3`
- observed balance: `149 -> 144`
- `tx_found: true`
- retained log: `$LP0008_TEST_ROOT/final_pre_video_evidence.log`

## Evidence table

| Criterion | Evidence |
|---|---|
| Loads inside Logos Core alongside wallet/storage/messaging without modifying those modules | `scripts/run_logoscore_integration.sh all` starts a fresh logoscore daemon and co-loads `agent_module`, `storage_module`, and `delivery_module`. |
| Agent has its own shielded LEZ account and can send/receive independently | `scripts/run_live_wallet_send_verify.py` mounts an rc3 funded wallet state, verifies the selected private account, submits `wallet.send`, polls the public testnet transaction, syncs, and verifies balance decreased. |
| Single-command headless deployment/configuration | `nix build .#install`; `logoscore --config-dir ... -m ./result/modules ... daemon`; `meta.configure` through `dispatchSkill`. |
| Owner interaction through Logos Messaging / no intermediary server | `scripts/run_logoscore_integration.sh stage-c` sends a valid LIP-23 message through `delivery_module` from a separate logoscore client process into the same headless daemon. `agent.receive` is implemented as persisted inbox polling because delivery exposes events but no synchronous receive/poll query API; see `docs/upstream/delivery-receive-poll-api.md`. |
| Spending threshold | `demo.sh` and `scripts/run_lp0008_deep_verify.py` prove below-threshold approval and above-threshold block. |
| All default skills | `demo.sh` and `scripts/run_lp0008_deep_verify.py` exercise all 23 skills through raw `logos_module_dispatch`. |
| A2A-compatible coordination | `agent.card` emits an A2A card; `scripts/run_multi_agent_a2a_demo.sh` proves three Agent Cards, discovery, inbound task processing, lifecycle events, subscribe readback, and terminal cancel guard. |
| 3 illustrative use cases | Personal file vault: `storage.upload/download/list/share`; agent services marketplace: A2A Agent Card plus live task-envelope transport and deterministic payment hook; multi-agent workflow: Alpha -> Beta task handoff and completion. Wallet funding/payment proof is separately covered by live LEZ wallet send; strict live task-linked inter-agent payment remains a residual gap. |
| Three separate agents | `scripts/run_multi_agent_a2a_demo.sh` configures Alpha Storage, Beta Messaging, and Gamma Chain identities with separate Agent Cards, task topics, owner topics, and roles. |
| Documentation and clean repo | README, SUBMISSION, this evidence map, dependency policy, upstream-gap docs, CI workflow, MIT license. |
| CI green | GitHub Actions builds and runs `demo.sh` on Linux. Linux wallet bridge fails closed instead of pretending Darwin wallet FFI is available. |
| Video | Not ready for final recording until strict gaps in `docs/submission-readiness-matrix.md` are closed or accepted as residual risk. |

## Current no-go items

- Separate Logos app/Basecamp owner-channel interaction is not yet proven.
- Above-threshold owner approval/reject retry/timeout flow is implemented at module level: `wallet.send` creates persisted pending approvals and owner-topic notifications; `approval.retry`, `approval.reject`, and `approval.approve` are verified by `demo.sh`. Remaining risk: reviewer-facing Basecamp UI/channel proof is still pending.
- A2A task transport over Logos Messaging is now proven by `scripts/run_lp0008_deep_verify.py` (`transport.result.mode == live`). Live task-linked LEZ payment between separate funded agents is not yet proven; `demo.sh` proves the task payment hook and lifecycle deterministically.
- Three separate LEZ testnet agent deployments and three full testnet use cases are not yet proven.
- CU cost and `RISC0_DEV_MODE=0`/proof-generation evidence remain to be completed or bounded.

## Honest boundaries

- The public-testnet wallet proof is real and uses the LEZ wallet FFI; it is not a mocked transaction.
- Linux CI does not pretend wallet FFI is available. It builds a fail-closed bridge for unsupported platforms.
- `program.call` and `program.deploy` fail closed inside the module until Logos exposes a module-safe LEZ program SDK/C ABI. The compatible external rc3 SPEL/lgs path is documented in `docs/upstream/program-live-api.md`.
- Delivery receive/poll is constrained by the current delivery module API; `agent.receive` processes the persisted inbox and live send is proven through `delivery_module`.

## CU/proof-mode documentation

`docs/cu-costs.md` documents current CU/proof boundaries: public-testnet wallet tx evidence exists, current rc3 wallet output does not expose a stable CU field, and `program.call`/`program.deploy` submit no transaction because they fail closed.

## Three illustrative use cases

`scripts/run_three_use_cases_demo.sh` exercises personal file vault, agent services marketplace payment-hook, and multi-agent workflow evidence, writing `summary.json` under the evidence directory. This is partial strict evidence: live A2A transport is separately proven by deep verify, while live inter-agent LEZ payment remains bounded by the wallet evidence path.

## Resilience and fail-closed evidence

`scripts/run_resilience_evidence.sh` proves persisted pending approvals reload across a new process, expired approvals fail closed on retry/approve, and a failed task does not poison subsequent storage skill execution. This is deterministic local resilience evidence, not a distributed network-partition proof.
