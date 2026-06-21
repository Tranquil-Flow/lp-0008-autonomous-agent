# LP-0008 strict submission-readiness matrix

Status: **not ready for final Lambda Prize submission yet**.

This matrix separates working implementation evidence from strict prize acceptance. It exists to prevent validator/CI success from being mistaken for complete acceptance readiness.

| Prize criterion | Current status | Evidence | Remaining work |
|---|---|---|---|
| Load agent inside Logos Core alongside storage/messaging/wallet without modifying those modules | Partial | `scripts/run_logoscore_integration.sh all` co-loads `agent_module`, `storage_module`, and `delivery_module`; wallet FFI path is available on Darwin. | Prove wallet as a co-loaded Logos Core module or keep Darwin FFI boundary explicit. |
| Agent has own shielded LEZ account and can send/receive tokens | Partial | rc3 wallet FFI funded send verified on public testnet, tx `5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3`. | Add receive-token evidence and document platform limits; Linux stays fail-closed. |
| Single CLI deploy/configure on any machine using headless Logos Core | Partial | Nix build + `logoscore` load + `meta.configure`. | Make platform prerequisites explicit; "any machine" is limited by wallet FFI availability. |
| Owner interacts in real time from separate Logos app/Basecamp via Logos Messaging, no intermediary server | Not yet met | Basecamp artifact readiness exists via `scaffold.toml` and `.lgx` build. | Verify/build Basecamp owner-channel UI flow and record separate Logos app instance interaction. |
| Spending threshold holds above-threshold for owner approval and executes below-threshold autonomously | Partial | Spending gate blocks above limit and permits below limit. | Implement owner notification, approve/reject commands, retry, timeout, and persistence. |
| All default skills implemented and documented | Partial | 23 dispatch handlers exist and are documented. | Close or clearly bound simulated/fail-closed skills: program query/call/deploy, share semantics, wallet history/source of truth, messaging groups. |
| A2A-compatible Agent Cards and task lifecycle over Logos Messaging | Partial | Agent Cards and local persisted task lifecycle exist. | Publish/discover cards and task envelopes over Logos Messaging transport; validate schema. |
| Two+ agents discover, execute task, and transfer LEZ payment autonomously | Not yet met | Local three-agent control-plane demo exists. | Add Logos Messaging discovery/task transport and confirmed LEZ payment tied to task acceptance. |
| Three illustrative use cases demonstrated end-to-end on LEZ testnet | Not yet met | Storage, local A2A, and wallet primitives have evidence. | Produce 3 full use-case scripts/evidence on testnet or document exact upstream blockers. |
| Three separate agents deployed on LEZ testnet, one per default skill category | Not yet met | Alpha/Beta/Gamma local configs/cards exist. | Deploy separate profiles/agents with reproducible evidence and category-specific skills. |
| Full documentation and clean public repo | Partial | README, docs, CI, MIT license, evidence docs. | Update docs after strict gaps close and remove this not-ready matrix from final claim surfaces or convert it to final evidence. |
| Skill interface allows adding skills without modifying core agent module | Partial | `SkillRegistry`/schemas exist. | Provide plugin/extension path or document required code changes honestly. |
| Basecamp owner-facing interface and loadable assets | Partial | `scaffold.toml`; `.lgx` build path. | Real Basecamp install/launch and owner-channel interaction proof. |
| Recovery from transient failures/node restarts without losing pending task state | Partial | File-backed persistence exists. | Add explicit restart/network interruption tests. |
| Above-threshold transactions unreachable owner: retry then timeout/fail closed | Not yet met | Above-threshold does not execute. | Add retry/timeout owner notification state machine. |
| Skill failures isolated | Partial | Dispatch catches exceptions and returns JSON errors. | Add concurrent/failing-skill isolation tests. |
| CU cost documented | Not yet met | Wallet tx evidence exists. | Add `docs/cu-costs.md` for token transfer and program operations/unavailable boundary. |
| LEZ devnet/testnet deployment/testing | Partial | Wallet send on public testnet; Logos Core local module integration. | Expand to full module/testnet use cases and three agents. |
| E2E tests against LEZ sequencer included in CI | Not yet met | CI builds and runs local demo. | Add opt-in or CI-safe sequencer/testnet gate; document secrets/network limits. |
| CI green on default branch | Met | GitHub Actions green on default branch. | Keep green after new work. |
| README documents CLI and Logos app owner-channel usage | Partial | CLI docs exist; Basecamp readiness docs exist. | Add true owner-channel guide after Basecamp proof. |
| Reproducible E2E demo script against local sequencer with `RISC0_DEV_MODE=0` | Not yet met | Demo scripts exist but do not prove this. | Add proof-mode/local-sequencer segment or document non-applicability/upstream blocker. |
| Recorded video demo | Not yet met | Scripts/narration prepared. | Record final terminal + Basecamp evidence after strict gaps close. |

## Current go/no-go

- Mechanical repository hygiene: **go**.
- Strict Lambda Prize final submission: **no-go** until the not-yet-met items above are closed or explicitly accepted as upstream-blocked risk by the user.
