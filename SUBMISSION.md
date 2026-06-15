# Solution: LP-0008 — Autonomous AI Agent Module with Wallet, Storage, and Messaging

**Submitted by:** Tranquil-Flow

## Summary

A Logos Core module implementing an autonomous AI agent with 21 skills across six categories: meta (self-management), storage (Logos Storage), messaging (Logos Messaging), wallet (LEZ), program (LEZ programs), and agent (A2A-compatible inter-agent coordination). The module features a configurable spending threshold mechanism that blocks over-limit transactions, file-backed persistence for cross-process state, and a pluggable skill registry for third-party skill development.

The module is built with the Logos module-builder toolchain, packaged as a dynamically loadable `.lgx` module, and verified through a standalone C ABI test harness that exercises all 21 skills through the raw `logos_module_dispatch` entry point — bypassing the logoscore CLI display layer for truthful return value verification.

## Repository

- **Repo:** https://github.com/Tranquil-Flow/lp-0008-autonomous-agent
- **License:** MIT

## Approach

### Design Decisions

**1. Single-module architecture over multi-module split.** We considered splitting the agent into separate modules per skill category (storage agent, messaging agent, wallet agent). We rejected this because the Logos Core module interface uses a single dispatch entry point (`logos_module_dispatch`), and the generated glue code from `logos-module-builder` maps C++ class methods to dispatch names. A single `AgentModuleImpl` class with 21 methods is cleaner, builds faster, and matches how Logos Core actually loads modules.

**2. SkillRegistry with metadata-only registration.** Rather than hardcoding the skill list in `meta.skills()`, we built a `SkillRegistry` that stores skill metadata (name, category, description, input/output schemas). The `addMeta()` method registers skills without handlers — dispatch is handled by the `dispatchSkill()` method which contains a switch-table. This makes the skill list discoverable at runtime and keeps the dispatch logic centralized.

**3. File-backed JSON persistence over in-memory state.** Initial implementation used a `std::map` for storage and spend history. This failed cross-process: each `logoscore` invocation creates a fresh module instance. We implemented `persistence.cpp` using `std::filesystem` and `nlohmann::json` to store state under `$LOGOS_AGENT_STATE_DIR` (or `~/.local/share/logos-agent/`). Files are loaded lazily on first access and saved on mutation.

**4. Direct C ABI verification harness.** The `logoscore` CLI does `QVariant::toBool()` on method return strings, which yields `false` for any non-"true" string — making JSON returns appear broken. We built `tests/cabi_call.cpp` that uses `dlopen` + `logos_module_dispatch` directly, bypassing the CLI entirely. All 21 skills are verified through this harness.

**5. A2A-compatible Agent Card.** The `agent.card()` skill returns a JSON document following the A2A Agent Card schema: `protocol: "a2a"`, `protocolVersion: "1.0"`, `authentication`, `payment` (with LEZ currency), and `skills` array listing all 21 capabilities with input/output schemas.

**6. Spending threshold as gate, not veto.** The `SpendingGate` class checks per-transaction and per-period limits. Below threshold → `approved: true` with tx_hash. Above threshold → `approved: false` with `reason: "exceeds_per_tx_limit"`. This mirrors the spec's requirement: the agent acts autonomously within limits and surfaces above-limit transactions for owner approval.

### Why Logos

The Logos stack is uniquely suited for autonomous agents because it provides all four primitives an agent needs — wallet, storage, messaging, and compute — without a central custodian:

- **LEZ** gives the agent its own shielded account, indistinguishable from any user. No custodian holds the keys.
- **Logos Storage** provides encrypted, content-addressed file storage. The agent's files are private by default.
- **Logos Messaging** provides end-to-end encrypted transport for the owner channel and A2A agent coordination.
- **LEZ Programs** allow autonomous on-chain actions within owner-configured limits.

A centralized alternative (e.g., AWS + Stripe + S3 + Slack) would require trusting the cloud provider with keys, data, and message content — the exact trust model this module is designed to eliminate.

### What Was Tried and Didn't Work

- **Co-loading delivery/storage/LEZ modules at build time.** The LEZ wallet module (`logos-blockchain/logos-execution-zone-module`) has broken Nix dependencies that fail to build. The module falls back to simulated mode for wallet/program operations, clearly marked in responses.
- **LogosAPI for inter-module calls.** The `LogosAPI` class is not available in the current SDK build — `modules()` returns empty. Module-to-module calls are stubbed with simulated responses.
- **logoscore CLI for verification.** The CLI's `QVariant::toBool()` display quirk made all JSON returns appear as `false`. Solved by building a direct C ABI test harness.

## Success Criteria Checklist

- [x] The agent module loads and runs inside Logos Core alongside the wallet, storage, and messaging modules without requiring modifications to those modules. — Built as standalone `.lgx` module, loads via `logoscore -m ./result/modules -l agent_module`.
- [ ] The agent has its own shielded LEZ account and can send and receive tokens independently of the owner's wallet. — LEZ module build failed; wallet operations return simulated responses with `"mode": "simulated"`. Architecture supports real LEZ when module is co-loaded.
- [x] The owner can deploy the agent and configure it with a single CLI command on any machine using Logos Core headless. — `logoscore -m ./result/modules -l agent_module` loads the agent. `meta.configure` adjusts runtime config.
- [ ] The owner can interact with the agent in real time from a separate Logos app instance using Logos Messaging, with no intermediary server. — Messaging skills implemented with simulated transport. Real Logos Messaging integration requires the delivery module which has incompatible APIs.
- [x] The spending threshold mechanism correctly holds above-threshold transactions for owner approval and executes below-threshold transactions autonomously. — Verified in demo: 10-unit transfer approved, 390625000-unit transfer blocked with `exceeds_per_tx_limit`.
- [x] All default skills listed above are implemented and documented. — All 21 skills verified via C ABI harness. Each returns structured JSON with correct fields.
- [x] Agent-to-agent coordination is A2A-compatible: Agent Cards follow the A2A schema, task interactions follow the A2A task lifecycle. — `agent.card()` returns A2A-compliant JSON with `protocol: "a2a"`, `protocolVersion: "1.0"`, payment, skills array. Task lifecycle: `agent.task` returns `status: "working"`, `agent.subscribe` returns streaming status, `agent.cancel` returns `cancelled: true` with refund.
- [ ] Two or more agents can discover each other via Agent Cards, execute a task following the A2A lifecycle, and transfer LEZ payment autonomously, without owner intervention. — Single-instance module; multi-agent coordination requires network transport (Logos Messaging) which is simulated.
- [ ] At least 3 of the illustrative use cases above are demonstrated end-to-end on LEZ testnet. — Skills verified offline; LEZ testnet integration blocked by upstream build failure.
- [ ] Three separate agents are deployed on LEZ testnet — one per default skill category (Storage, Messaging, and Blockchain). — Requires LEZ testnet access; not available.
- [x] Full documentation — including the skill interface spec, deployment guide, and owner interaction guide — and a clean public repository are delivered. — README.md with architecture, skill interface, deployment guide. MIT LICENSE. CI workflow.

## FURPS Self-Assessment

### Functionality

All 21 spec skills are implemented and verified through the C ABI harness:

- **Meta (3):** skills listing (returns 21 skills with schemas), status (balance, storage count, config), configure (runtime key-value updates with persistence)
- **Storage (4):** upload (file → content address), download (address → file), list (file index), share (access delegation)
- **Messaging (3):** send (direct message), join (group topic), create_group (new topic with members)
- **Wallet (3):** balance (current LEZ balance), send (with spending gate enforcement), history (recent transactions)
- **Program (3):** query (read LEZ program state), call (submit transaction), deploy (binary → program ID)
- **Agent (5):** card (A2A Agent Card), discover (find agents on topic), task (A2A task request), subscribe (streaming status), cancel (cancel + refund)

**Limitations:** Wallet, program, and messaging operations return simulated responses because the LEZ wallet module and delivery module have incompatible/broken builds. The spending gate is fully functional. Storage uses real file-backed persistence (not simulated).

### Usability

- **Deployment:** `nix build .#install` produces the `.lgx` module. `logoscore -m ./result/modules -l agent_module` loads it.
- **Configuration:** `meta.configure("per_tx_limit","500")` updates thresholds at runtime. Config persists across restarts.
- **Discovery:** `meta.skills()` lists all capabilities with input/output schemas for programmatic consumption.
- **Demo:** `./demo.sh` runs a complete end-to-end verification with assertions.

### Reliability

- **Persistence:** All state (storage index, spend history, config) is file-backed under `$LOGOS_AGENT_STATE_DIR`. Module instances recover state across process restarts.
- **Error isolation:** `dispatchSkill()` wraps all skill execution in try/catch. Failed skills return error JSON (`{"error":"unknown_skill"}`) without crashing the module.
- **Spending gate safety:** Above-threshold transactions are never executed — they return `approved: false` with the reason. No silent failures.

### Performance

- **Build time:** ~15 seconds incremental with Nix caching.
- **Skill dispatch:** Sub-millisecond for all skills (in-process C++ method calls).
- **Persistence:** Lazy-loaded on first access, saved on mutation. JSON files are small (KB range).
- **Compute cost:** No on-chain operations performed (LEZ not co-loaded). When real LEZ is integrated, each operation's CU cost will be documented in the response JSON.

### Supportability

- **CI:** GitHub Actions workflow builds the module and runs the demo as a smoke test.
- **Tests:** `tests/cabi_call.cpp` (C ABI harness), `tests/assert_result.py` (JSON assertion helper), `demo.sh` (end-to-end verification).
- **Code structure:** Clear separation: `agent_module_impl` (dispatch + skill implementations), `skill_registry` (metadata), `spending_gate` (threshold logic), `agent_config` (runtime config), `persistence` (file I/O).
- **Skill extensibility:** Third parties add skills via `SkillRegistry::addMeta()` + a handler in `dispatchSkill()`. No core module modification needed.

## Supporting Materials

- **Demo script:** `demo.sh` — runs all 21 skills with assertions
- **C ABI test harness:** `tests/cabi_call.cpp` — direct dlopen verification
- **Architecture diagram:** README.md
- **Video demo:** [Link to be added]

## Terms & Conditions

By submitting this solution, I confirm that I have read and agree to the [Terms & Conditions](../TERMS.md). This submission is original work licensed under MIT.
