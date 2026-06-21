# LP-0008 three illustrative use cases

This document maps the three reviewer-facing use cases to executable evidence. It is intentionally precise about what is live, deterministic, simulated, or still strict-risk.

Primary executable gate:

- `scripts/run_three_use_cases_demo.sh`

The script writes evidence under:

- `$LP0008_USE_CASE_STATE` if set, otherwise `~/lp0008-phase0/three_use_cases_<timestamp>/`

## Use case 1: personal file vault

Flow:

1. Create a local secret file.
2. Run `storage.upload` with label `personal-vault`.
3. Run `storage.download` to a separate path.
4. Byte-compare original and restored file.
5. Run `storage.list` and `storage.share` to grant metadata-level access to `beta-messaging`.

Evidence files:

- `usecase1-upload.out`
- `usecase1-download.out`
- `usecase1-list.out`
- `usecase1-share.out`
- `vault-secret.txt`
- `vault-secret-restored.txt`

Strict boundary:

- Upload/download/list use the module's current storage path. Full cryptographic third-party share semantics remain bounded as metadata-level sharing unless/until Logos Storage exposes a stronger cross-identity grant API to this module.

## Use case 2: agent services marketplace

Flow:

1. Configure a marketplace buyer Agent Card.
2. Configure per-task LEZ payment metadata.
3. Publish/read the Agent Card and verify payment fields.
4. Submit an A2A task using `agent.task`.
5. Verify task lifecycle reaches `completed`.
6. Verify `payment_submitted` is recorded and `wallet.history` contains a `task.payment` entry.

Evidence files:

- `usecase2-card.out`
- `usecase2-task.out`
- `usecase2-history.out`

Strict boundary:

- The deterministic demo defaults `LP0008_DISABLE_WALLET_FFI=1`, so the marketplace payment hook records a simulated transaction unless the environment explicitly enables the wallet FFI and provides funded-wallet configuration. Live standalone wallet send evidence is separately recorded by `scripts/run_live_wallet_send_verify.py`.

## Use case 3: multi-agent workflow

Flow:

1. Configure and publish three Agent Cards:
   - Alpha Storage
   - Beta Messaging
   - Gamma Chain
2. Discover the three cards from `/logos/agents/v1/discovery`.
3. Send an inbound A2A task envelope from Alpha to Beta's task topic.
4. Run `agent.receive` as Beta.
5. Verify queued -> working -> completed task lifecycle and delegated message result.

Evidence files:

- `usecase3-alpha-card.out`
- `usecase3-beta-card.out`
- `usecase3-gamma-card.out`
- `usecase3-discover.out`
- `usecase3-inbound.out`
- `usecase3-receive.out`

Strict boundary:

- This is a deterministic control-plane proof using three persisted agent identities. Live Logos Messaging A2A transport is separately proven in `scripts/run_lp0008_deep_verify.py` via `transport.result.mode == live`.

## Summary output

At the end of a successful run, the script writes `summary.json` and prints:

`ASSERT three illustrative use cases: personal vault, marketplace payment hook, multi-agent workflow: OK`
