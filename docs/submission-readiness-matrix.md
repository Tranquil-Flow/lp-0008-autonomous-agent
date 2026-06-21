# LP-0008 strict submission-readiness matrix

Status: **not ready for final Lambda Prize submission yet**.

This matrix separates working implementation evidence from strict prize acceptance. It exists to prevent validator/CI success from being mistaken for complete acceptance readiness.

| Prize criterion | Current status | Evidence | Remaining work |
|---|---|---|---|
| Load agent inside Logos Core alongside storage/messaging/wallet without modifying those modules | Partial | `scripts/run_logoscore_integration.sh all` co-loads `agent_module`, `storage_module`, and `delivery_module`; wallet FFI path is available on Darwin. | Prove wallet as a co-loaded Logos Core module or keep Darwin FFI boundary explicit. |
| Agent has own shielded LEZ account and can send/receive tokens | Partial | rc3 wallet FFI funded send verified on public testnet, tx `5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3`. | Add receive-token evidence and document platform limits; Linux stays fail-closed. |
| Single CLI deploy/configure on any machine using headless Logos Core | Partial | Nix build + `logoscore` load + `meta.configure`. | Make platform prerequisites explicit; "any machine" is limited by wallet FFI availability. |
| Owner interacts in real time from separate Logos app/Basecamp via Logos Messaging, no intermediary server | Not yet met | Basecamp artifact readiness exists via `scaffold.toml` and `.lgx` build. | Verify/build Basecamp owner-channel UI flow and record separate Logos app instance interaction. |
| Spending threshold holds above-threshold for owner approval and executes below-threshold autonomously | Mostly closed | Spending gate permits below-limit sends; above-limit sends create persisted approval records, send owner-topic notifications, support retry/reject/approve, enforce timeout on approval/retry, and record approved execution in wallet history. | Still need Basecamp owner-channel UI proof for reviewer-facing approval. |
| All default skills implemented and documented | Partial | 23 dispatch handlers exist and are documented. | Close or clearly bound simulated/fail-closed skills: program query/call/deploy, share semantics, wallet history/source of truth, messaging groups. |
| A2A-compatible Agent Cards and task lifecycle over Logos Messaging | Mostly closed | Agent Cards are published to the discovery registry; `scripts/run_lp0008_deep_verify.py` proves `agent.task` emits a task envelope over live `delivery_module`/Logos Messaging and persists queued -> transport_sent -> working -> completed. | Add stricter schema validation and separate-process receive polling if delivery exposes a receive API. |
| Two+ agents discover, execute task, and transfer LEZ payment autonomously | Partial | `scripts/run_lp0008_deep_verify.py` proves `agent.task` emits an A2A task envelope over live `delivery_module`/Logos Messaging (`transport.result.mode == live`) and completes the requested skill. `demo.sh` proves the per-task LEZ payment hook is recorded in task lifecycle with `payment_submitted`. | Upgrade from deterministic/simulated payment-hook evidence to a confirmed live LEZ payment tied to task acceptance between separate funded agents. |
| Three illustrative use cases demonstrated end-to-end on LEZ testnet | Partial | `scripts/run_three_use_cases_demo.sh` now proves personal file vault, agent services marketplace payment hook, and multi-agent workflow evidence with `summary.json`. Live Logos Messaging A2A transport is proven separately by deep verify; live LEZ payment remains separate wallet evidence. | Upgrade the marketplace payment hook to a confirmed live LEZ transfer between separate funded agents, and record reviewer-facing video. |
| Three separate agents deployed on LEZ testnet, one per default skill category | Partial | `scripts/run_three_use_cases_demo.sh` and `scripts/run_multi_agent_a2a_demo.sh` publish Alpha Storage, Beta Messaging, and Gamma Chain Agent Cards with separate task/owner topics. | Turn the deterministic three-profile proof into three separately funded/testnet runtime deployments if reviewers require separate wallet identities. |
| Full documentation and clean public repo | Partial | README, docs, CI, MIT license, evidence docs. | Update docs after strict gaps close and remove this not-ready matrix from final claim surfaces or convert it to final evidence. |
| Skill interface allows adding skills without modifying core agent module | Partial | `SkillRegistry`/schemas exist. | Provide plugin/extension path or document required code changes honestly. |
| Basecamp owner-facing interface and loadable assets | Partial | `scaffold.toml`; `.lgx` build path. | Real Basecamp install/launch and owner-channel interaction proof. |
| Recovery from transient failures/node restarts without losing pending task state | Partial | File-backed persistence exists. | Add explicit restart/network interruption tests. |
| Above-threshold transactions unreachable owner: retry then timeout/fail closed | Mostly closed | Above-threshold sends create pending approvals, send owner-topic notifications, retry notifications, reject/approve, and fail closed on expired approvals. | Still needs Basecamp owner-channel UI proof for the owner-facing approval surface. |
| Skill failures isolated | Partial | Dispatch catches exceptions and returns JSON errors. | Add concurrent/failing-skill isolation tests. |
| CU cost documented | Partial | `docs/cu-costs.md` records the latest public-testnet wallet tx and states that current rc3 wallet output does not expose a stable CU field; program ops fail closed with no submitted tx. | Add measured CU numbers if/when Logos tooling exposes them, and add proof-mode evidence if required by reviewers. |
| LEZ devnet/testnet deployment/testing | Partial | Wallet send on public testnet; Logos Core local module integration. | Expand to full module/testnet use cases and three agents. |
| E2E tests against LEZ sequencer included in CI | Not yet met | CI builds and runs local demo. | Add opt-in or CI-safe sequencer/testnet gate; document secrets/network limits. |
| CI green on default branch | Met | GitHub Actions green on default branch. | Keep green after new work. |
| README documents CLI and Logos app owner-channel usage | Partial | CLI docs exist; Basecamp readiness docs exist. | Add true owner-channel guide after Basecamp proof. |
| Reproducible E2E demo script against local sequencer with `RISC0_DEV_MODE=0` | Not yet met | Demo scripts exist but do not prove this. | Add proof-mode/local-sequencer segment or document non-applicability/upstream blocker. |
| Recorded video demo | Not yet met | Scripts/narration prepared. | Record final terminal + Basecamp evidence after strict gaps close. |

## Current go/no-go

- Mechanical repository hygiene: **go**.
- Strict Lambda Prize final submission: **no-go** until the not-yet-met items above are closed or explicitly accepted as upstream-blocked risk by the user.
