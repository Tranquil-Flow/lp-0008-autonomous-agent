# LP-0008 strict success-criteria evidence map

This file maps each LP-0008 success criterion to a command, artifact, or honest boundary. It is intended for reviewers and for the final pre-video checklist.

## Final pre-video gate

Run:

```bash
scripts/run_final_pre_video_evidence.sh
```

With a funded rc3 wallet mounted, also export:

```bash
export LP0008_LIVE_WALLET_HOME=$HOME/lp0008-phase0/rc3_faucet_wallet
export LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq
export LP0008_LIVE_SEND_RECIPIENT=yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3
```

The gate runs build, raw C ABI verification, Logos Core co-load integration, three-agent A2A proof, readiness validation, and the optional live wallet-send verifier.

Latest full pre-video gate run on the M4 Pro completed with `PRE_VIDEO_EVIDENCE_OK` and real public-testnet wallet proof:

- tx: `5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3`
- account: `27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq`
- recipient: `yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3`
- observed balance: `149 -> 144`
- `tx_found: true`
- retained log: `~/lp0008-phase0/final_pre_video_evidence.log`

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
| 3 illustrative use cases | Personal file vault: `storage.upload/download/list/share`; agent services marketplace: three Agent Cards plus delegated task; multi-agent workflow: Alpha -> Beta task handoff and completion. Wallet funding/payment proof is separately covered by live LEZ wallet send. |
| Three separate agents | `scripts/run_multi_agent_a2a_demo.sh` configures Alpha Storage, Beta Messaging, and Gamma Chain identities with separate Agent Cards, task topics, owner topics, and roles. |
| Documentation and clean repo | README, SUBMISSION, this evidence map, dependency policy, upstream-gap docs, CI workflow, MIT license. |
| CI green | GitHub Actions builds and runs `demo.sh` on Linux. Linux wallet bridge fails closed instead of pretending Darwin wallet FFI is available. |
| Video | Pending final narrated recording only. |

## Honest boundaries

- The public-testnet wallet proof is real and uses the LEZ wallet FFI; it is not a mocked transaction.
- Linux CI does not pretend wallet FFI is available. It builds a fail-closed bridge for unsupported platforms.
- `program.call` and `program.deploy` fail closed inside the module until Logos exposes a module-safe LEZ program SDK/C ABI. The compatible external rc3 SPEL/lgs path is documented in `docs/upstream/program-live-api.md`.
- Delivery receive/poll is constrained by the current delivery module API; `agent.receive` processes the persisted inbox and live send is proven through `delivery_module`.
