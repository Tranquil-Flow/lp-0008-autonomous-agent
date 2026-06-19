# Acceptance gap audit

Status captured for LP-0008 after the inbound A2A receive slice.

## Closed

- Agent module builds with `nix build .#install`.
- `metadata.json` keeps dependencies as [] for standalone/progressive Logos Core loading.
- Full-stack co-load is verified through `scripts/run_logoscore_integration.sh` with agent + storage + delivery loaded together.
- Storage upload/download/list use live `storage_module` when co-loaded and verify byte-for-byte round trip.
- Messaging send uses live `delivery_module` for valid LIP-23 content topics.
- Invalid/local topics are guarded before reaching delivery, preventing the known delivery invalid-topic abort.
- Real LEZ wallet FFI initializes a shielded testnet account and reports live balance/history status when the wallet is available.
- Funded wallet send is exercised on public testnet by `scripts/run_live_wallet_send_verify.py`: rc3 Pinata faucet funding, mounted private wallet, FFI send, transaction lookup, and balance decrease.
- Spending gate blocks above-threshold sends before execution.
- A2A Agent Cards, discovery registry, task lifecycle, `agent.complete`, and `agent.receive` are persisted and verified by raw CLI/deep harnesses.
- Three configured agents are exercised by the multi-agent A2A demo in Logos Core/C ABI form.

## Residual

- Program deploy/query/call has no in-process LEZ program SDK/C ABI available to a Logos Core module; `program.call` and `program.deploy` fail closed and the rc3 SPEL/lgs external path is documented.
- Delivery has `messageReceived` events but no synchronous receive/poll query API for headless module-to-module retrieval, so `agent.receive` currently verifies persisted-inbox polling rather than a live background delivery subscriber.
- Final public submission still needs a fresh demo video URL and explicit approval before any Logos PR is opened.

## Acceptance posture

The repository should not overclaim residuals. The acceptance strategy is to maximize real proof where infrastructure exists, preserve raw evidence for every closed claim, and document upstream-dependent gaps precisely rather than hiding them.
