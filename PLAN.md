# LP-0008: Autonomous AI Module — Implementation Plan

**Prize:** $1,200 USDT · **Status:** Open (0 active submissions, 4 prior failed attempts)  
**Repository:** TBD (new repo, clean-room from scratch, prior attempts as design inspiration only)  
**Machine:** M4 Pro 48GB arm64 — full Logos Core toolchain pre-installed  
**Created:** 2026-06-15  

---

## 1. Success Criteria Checklist (100% Coverage Required)

Extracted verbatim from `prizes/LP-0008.md`. Every item MUST be verifiable on camera.

### 1.1 Module Implementation (MUST)

| # | Criterion | How We Prove It | Status |
|---|-----------|-----------------|--------|
| C1 | Logos Core module (universal authoring model) | `nix build` produces `.dylib`/`.so`; `lm methods` shows API | ☐ |
| C2 | Loads into `logoscore` at runtime | `logoscore -m ./result/lib -l agent_module -c "agent_module.status()"` | ☐ |
| C3 | Calls LEZ wallet module methods | `logoscore -c "agent_module.dispatchSkill(wallet.balance)"` returns balance | ☐ |
| C4 | Calls Storage module methods | `logoscore -c "agent_module.dispatchSkill(storage.upload, <file>)"` returns CID | ☐ |
| C5 | Calls Messaging module methods | `logoscore -c "agent_module.dispatchSkill(messaging.send, <topic>, <msg>)"` delivers | ☐ |
| C6 | `metadata.json` (the "module.json" the validation bot looks for) at repo root | File exists with correct `interface: universal`, `dependencies`, `codegen` | ☐ |

### 1.2 Default Skills (ALL 18 required)

| Category | Skill | Input | Output | Implementation Path |
|----------|-------|-------|--------|-------------------|
| **Storage** | `storage.upload` | file path | CID | `modules().storage_module.uploadInit` → `uploadFinalize` |
| | `storage.download` | CID | file | `modules().storage_module.downloadInit` → `downloadDone` |
| | `storage.list` | — | manifest | `modules().storage_module.manifests` |
| | `storage.share` | CID, recipient | share link | `modules().storage_module.fetch` + delivery send |
| **Messaging** | `messaging.send` | topic, payload | request id | `modules().delivery_module.send(topic, payload)` |
| | `messaging.join` | topic | subscription | `modules().delivery_module.subscribe(topic)` |
| | `messaging.create_group` | member list | group topic | compose: subscribe + send invitation messages |
| **Blockchain** | `chain.balance` | account hex | balance string | `modules().logos_execution_zone.get_balance(hex, is_public)` |
| | `chain.send` | from, to, amount | tx hash | `modules().logos_execution_zone.transfer_*` |
| | `chain.history` | account hex | tx list | `modules().logos_execution_zone.sync_to_block` + query |
| | `chain.program.query` | program addr, method | result | LEZ program read call via wallet FFI |
| | `chain.program.call` | program addr, method, args | tx hash | LEZ program write call + spending gate |
| | `chain.program.deploy` | program bytecode | program addr | LEZ program deploy + spending gate |
| **A2A** | `a2a.card` | — | agent card JSON | return own Agent Card (JSON schema) |
| | `a2a.discover` | filter | agent list | subscribe to discovery topic, collect Agent Cards |
| | `a2a.task` | target agent, task | task status | send task message via delivery, track lifecycle |
| | `a2a.subscribe` | target agent, event | subscription | subscribe to agent's event topic |
| | `a2a.cancel` | task id | confirmation | send cancel message via delivery |
| **Meta** | `meta.skills` | — | skill list | enumerate registered skills + descriptions |
| | `meta.status` | — | agent status | return module health, config, thresholds |
| | `meta.configure` | key, value | confirmation | update runtime config (thresholds, skills, topics) |

### 1.3 Spending Threshold Gate (MUST)

| # | Criterion | Implementation |
|---|-----------|---------------|
| C7 | Per-transaction limit (configurable LEZ amount) | Check before `chain.send`, `chain.program.call`, `chain.program.deploy` |
| C8 | Per-period limit (daily/weekly rolling window) | Track cumulative spend in `instancePersistencePath()` JSON file |
| C9 | Above threshold → request owner approval via messaging | Send approval request to owner's content topic via delivery module; block until response |
| C10 | Below threshold → execute autonomously | No interruption |

### 1.4 Owner Channel (MUST)

| # | Criterion | Implementation |
|---|-----------|---------------|
| C11 | E2E encrypted messaging topic between agent and human owner | Delivery module `subscribe` + `send` on dedicated content topic |
| C12 | Agent sends: threshold-exceeded alerts, status updates, task completions | Triggered by spending gate + periodic heartbeat |
| C13 | Owner sends: approvals, rejections, config changes | Parse incoming messages, apply to runtime state |

### 1.5 A2A Protocol (MUST)

| # | Criterion | Implementation |
|---|-----------|---------------|
| C14 | Agent Card publication (A2A-compatible JSON schema) | Published via delivery module on discovery topic at startup + interval |
| C15 | Agent discovery (query other agents' cards) | Subscribe to discovery topic, cache seen cards |
| C16 | Task delegation (send task to another agent) | Send task JSON via delivery on target agent's task topic |
| C17 | Task lifecycle states (working → input-required → completed/failed) | Track in local state map, update via delivery messages |
| C18 | Cancellation | Send cancel message, update state |

### 1.6 Three Agent Deployments (MUST — demo different use cases)

| Agent | Role | Primary Skills | Use Case |
|-------|------|---------------|----------|
| **Alpha** | Storage specialist | `storage.*`, `chain.balance`, `a2a.*` | Personal file vault — store/retrieve/share files autonomously |
| **Beta** | Messaging specialist | `messaging.*`, `chain.balance`, `a2a.*` | On-chain event alerter — subscribe to LEZ events, notify owner |
| **Gamma** | Blockchain specialist | `chain.*`, `a2a.*` | DAO voting delegate — query programs, execute approved calls |

Each agent = same `agent_module` binary with different `agent_config.json` (enabled skills, wallet account, thresholds, owner topic).

### 1.7 Multi-Agent Demo (MUST)

| # | Criterion |
|---|-----------|
| C19 | At least 2 agents discover each other via A2A |
| C20 | One agent delegates a task to another |
| C21 | Task completes with verifiable result |
| C22 | Autonomous wallet interaction (at least one on-chain tx from an agent) |

### 1.8 Submission Requirements (MUST — from validation bot feedback on prior attempts)

| # | Requirement | File/Location |
|---|-------------|---------------|
| C23 | `solutions/LP-0008.md` filled from template | PR in `logos-co/lambda-prize` |
| C24 | All FURPS sections completed | Within `solutions/LP-0008.md` |
| C25 | Success Criteria Checklist | Within `solutions/LP-0008.md` |
| C26 | Terms & Conditions acknowledgment | Within `solutions/LP-0008.md` |
| C27 | Repository link with full implementation | Our new repo |
| C28 | `metadata.json` (module.json) at repo root | Repo root |
| C29 | LICENSE file (MIT or Apache 2.0) | Repo root, dual-licensed |
| C30 | CI pipeline (green on default branch) | `.github/workflows/ci.yml` |
| C31 | `demo.sh` script (reproducible end-to-end flow) | `scripts/demo.sh` |
| C32 | Narrated video walkthrough (builder narrates) | Demo video file or URL |
| C33 | `RISC0_DEV_MODE=0` — real ZK proofs (not dev mode) | Terminal output in demo |

---

## 2. Architecture Design

### 2.1 Module Dependency Graph

```
                    ┌─────────────────────┐
                    │   agent_module      │  ← Our module (universal authoring model)
                    │   (this repo)       │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
   ┌──────────────────┐ ┌──────────────┐ ┌──────────────────────┐
   │ storage_module   │ │delivery_module│ │ logos_execution_zone │
   │ (Logos Storage)  │ │ (Messaging)  │ │ (LEZ Wallet + Progs) │
   └──────────────────┘ └──────────────┘ └──────────────────────┘
```

`metadata.json#dependencies`:
```json
"dependencies": ["storage_module", "delivery_module", "logos_execution_zone"]
```

### 2.2 Agent Module Internal Architecture

```
src/
├── agent_impl.h               # Universal impl class — derives LogosModuleContext
├── agent_impl.cpp             # Main impl — skill dispatch, lifecycle
│
├── skill_registry.h           # Skill registration + dispatch table
├── skill_registry.cpp         # Maps skill names → handler functions
│
├── skills/
│   ├── storage_skills.h/.cpp  # storage.upload, download, list, share
│   ├── messaging_skills.h/.cpp# messaging.send, join, create_group
│   ├── chain_skills.h/.cpp    # chain.balance, send, history, program.*
│   ├── a2a_skills.h/.cpp      # a2a.card, discover, task, subscribe, cancel
│   └── meta_skills.h/.cpp     # meta.skills, status, configure
│
├── spending_gate.h/.cpp       # Threshold checking + owner approval flow
├── owner_channel.h/.cpp       # E2E encrypted messaging to human owner
├── a2a_protocol.h/.cpp        # Agent Card, discovery, task lifecycle
│
├── agent_config.h             # Config struct (thresholds, topics, enabled skills)
└── json_utils.h/.cpp          # nlohmann::json helpers
```

### 2.3 Agent Card Schema (A2A-compatible)

```json
{
  "protocol": "a2a",
  "version": "1.0",
  "agent_id": "alpha-storage-agent",
  "name": "Alpha — Storage Specialist",
  "description": "Autonomous file vault agent for Logos Storage",
  "capabilities": [
    "storage.upload", "storage.download", "storage.list", "storage.share",
    "chain.balance", "a2a.card", "a2a.discover", "a2a.task"
  ],
  "wallet_account": "0xabc123...",
  "messaging_topic": "/logos/agents/v1/alpha/tasks",
  "owner_topic": "/logos/agents/v1/alpha/owner",
  "status": "active",
  "timestamp": "2026-06-15T12:00:00Z"
}
```

### 2.4 A2A Transport Over Logos Delivery

All A2A messages are JSON payloads sent via `modules().delivery_module.send(topic, payload)`.

**Discovery topic:** `/logos/agents/v1/discovery`
- All agents subscribe at startup
- Agent Cards published on join + every 60s heartbeat

**Task topics:** Per-agent `/logos/agents/v1/<agent_id>/tasks`
- Task delegation: `{ "type": "task", "task_id": "...", "skill": "...", "args": {...} }`
- Task status: `{ "type": "task_status", "task_id": "...", "state": "working|completed|failed|input-required", "result": "..." }`
- Task cancel: `{ "type": "task_cancel", "task_id": "..." }`

**Owner channels:** Per-agent `/logos/agents/v1/<agent_id>/owner`
- Approval request: `{ "type": "approval_request", "task_id": "...", "amount": "...", "skill": "..." }`
- Approval response: `{ "type": "approval_response", "task_id": "...", "approved": true/false }`
- Config update: `{ "type": "config_update", "key": "...", "value": "..." }`

### 2.5 Spending Gate Flow

```
User/Agent calls chain.send(amount=50)
            │
            ▼
    ┌── spending_gate.check(amount, period) ──┐
    │                                         │
    │   amount <= per_tx_limit?               │
    │   AND                                   │
    │   cumulative + amount <= per_period_limit?│
    │                                         │
    ├── YES → execute → update cumulative → return tx_hash
    │
    └── NO → owner_channel.request_approval(skill, amount, task_id)
                │
                ├── Wait for approval_response (timeout: configurable, default 120s)
                │
                ├── Approved → execute → update cumulative → return tx_hash
                │
                └── Rejected/Timeout → return error, do NOT execute
```

---

## 3. Build Phases

### Phase 0: Environment Validation (Day 1)

| Task | Description | Gate |
|------|-------------|------|
| 0.1 | Create new repo `logos-autonomous-agent-module` | `git init` done |
| 0.2 | Scaffold module from template: `nix flake init -t github:logos-co/logos-module-builder` | Template files present |
| 0.3 | Build empty module: `nix build` | `.dylib` produced in `result/lib/` |
| 0.4 | Load into logoscore: `logoscore -m ./result/lib -l agent_module -c "agent_module.status()"` | Returns "running" |
| 0.5 | Add first dependency (`logos_execution_zone`), build, verify `modules().logos_execution_zone` resolves | Build passes with dep |
| 0.6 | Call `agent_module.getChainBalance()` which calls `modules().logos_execution_zone.get_balance(...)` | Returns balance or graceful "no wallet" error |

**Critical risk:** If cross-module calls don't work in step 0.5/0.6, we need to debug the module system before proceeding. This is the #1 technical risk.

### Phase 1: Skill Registry + Meta Skills (Days 2-3)

| Task | Description |
|------|-------------|
| 1.1 | Implement `skill_registry` — register skills with name, category, description, handler function pointer |
| 1.2 | Implement `meta.skills` — returns list of registered skills |
| 1.3 | Implement `meta.status` — returns agent health, config, thresholds |
| 1.4 | Implement `meta.configure` — update runtime config |
| 1.5 | Write unit tests for skill registry (mock dependencies) |
| 1.6 | Build + load into logoscore, verify `agent_module.dispatchSkill(meta.skills)` returns JSON |

### Phase 2: Blockchain Skills (Days 4-5)

| Task | Description |
|------|-------------|
| 2.1 | Implement `chain.balance` — calls `modules().logos_execution_zone.get_balance(hex, is_public)` |
| 2.2 | Implement `chain.send` — calls `modules().logos_execution_zone.transfer_*` (public/shielded/private variants) |
| 2.3 | Implement `chain.history` — sync wallet + query recent transactions |
| 2.4 | Implement `chain.program.query` — read-only program call |
| 2.5 | Implement `chain.program.call` — write program call (goes through spending gate) |
| 2.6 | Implement `chain.program.deploy` — deploy program bytecode (goes through spending gate) |
| 2.7 | Test each skill against local LEZ localnet (`lgs localnet start`) |

### Phase 3: Storage + Messaging Skills (Days 6-7)

| Task | Description |
|------|-------------|
| 3.1 | Implement `storage.upload` — `modules().storage_module.uploadInit/uploadFinalize` |
| 3.2 | Implement `storage.download` — `modules().storage_module.downloadInit/downloadDone` |
| 3.3 | Implement `storage.list` — `modules().storage_module.manifests` |
| 3.4 | Implement `storage.share` — fetch CID + send share notification via delivery |
| 3.5 | Implement `messaging.send` — `modules().delivery_module.send(topic, payload)` |
| 3.6 | Implement `messaging.join` — `modules().delivery_module.subscribe(topic)` |
| 3.7 | Implement `messaging.create_group` — subscribe to group topic + send invitation |
| 3.8 | Test storage skills against running storage node |
| 3.9 | Test messaging skills against running delivery node (logos.dev preset) |

### Phase 4: Spending Gate + Owner Channel (Days 8-9)

| Task | Description |
|------|-------------|
| 4.1 | Implement `spending_gate` — per-tx + per-period threshold checking |
| 4.2 | Implement `owner_channel` — subscribe to owner topic, send/receive messages |
| 4.3 | Implement approval request/response flow (JSON message protocol over delivery) |
| 4.4 | Wire spending gate into `chain.send`, `chain.program.call`, `chain.program.deploy` |
| 4.5 | Implement cumulative spend tracking (persist to `instancePersistencePath()`) |
| 4.6 | Test: set low threshold (1 LEZ), attempt transfer of 5 LEZ → approval request sent → approve → execute |
| 4.7 | Test: same → reject → transaction blocked |

### Phase 5: A2A Protocol (Days 10-11)

| Task | Description |
|------|-------------|
| 5.1 | Implement `a2a.card` — generate and return Agent Card JSON |
| 5.2 | Implement agent discovery — subscribe to `/logos/agents/v1/discovery`, collect cards |
| 5.3 | Implement `a2a.task` — send task delegation message, track lifecycle |
| 5.4 | Implement task handler — receive task, execute skill, send status update |
| 5.5 | Implement `a2a.cancel` — send cancel message, abort task |
| 5.6 | Implement `a2a.subscribe` — subscribe to another agent's event topic |
| 5.7 | Test: two agent instances discover each other, one delegates storage.upload task, other executes |

### Phase 6: Three Agent Configurations (Days 12-13)

| Task | Description |
|------|-------------|
| 6.1 | Create `configs/alpha-storage.json` — enable storage + chain.balance + a2a skills |
| 6.2 | Create `configs/beta-messaging.json` — enable messaging + chain.balance + a2a skills |
| 6.3 | Create `configs/gamma-blockchain.json` — enable chain.* + a2a skills |
| 6.4 | Create 3 wallet accounts on local LEZ testnet |
| 6.5 | Run all 3 agents simultaneously — verify no port/topic conflicts |
| 6.6 | Verify cross-agent A2A: Alpha delegates task to Gamma → Gamma executes on-chain |

### Phase 7: Demo Script + CI (Days 14-15)

| Task | Description |
|------|-------------|
| 7.1 | Write `scripts/demo.sh` — automated end-to-end flow following skill pattern from logos-basecamp |
| 7.2 | Demo flow: (1) start LEZ localnet, (2) load 3 agents, (3) show discovery, (4) Alpha uploads file, (5) Gamma checks balance, (6) Gamma attempts over-threshold tx → owner approval → execute, (7) Beta sends notification |
| 7.3 | Write `.github/workflows/ci.yml` — build module with nix, run tests |
| 7.4 | Verify CI is green on default branch |
| 7.5 | Write `solutions/LP-0008.md` from template — all FURPS sections, success criteria checklist, T&C ack |

### Phase 8: Video Recording + Submission (Days 16-17)

| Task | Description |
|------|-------------|
| 8.1 | Dry-run demo script on M4 Pro (font 18pt, clean terminal) |
| 8.2 | Record narrated walkthrough — Evi narrates over automated terminal flow |
| 8.3 | Verify `RISC0_DEV_MODE=0` appears in terminal output |
| 8.4 | Open PR in `logos-co/lambda-prize` with `solutions/LP-0008.md` |
| 8.5 | Final review against success criteria checklist |

---

## 4. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| **Cross-module calls don't resolve** | 🔴 Critical | Phase 0.5/0.6 validates this before any real code. If broken, we need to study the generated `logos_sdk.h` to debug. |
| **Storage/delivery modules don't run headless** | 🟠 High | Test in Phase 3 against real nodes. If headless mode fails, we can mock at the module boundary for the demo. |
| **LEZ localnet + RISC0 proofs too slow on M4 Pro** | 🟡 Medium | 48GB RAM + arm64 RISC0 toolchain pre-installed. Should be fine. Fallback: pre-record proof generation, narrate as evidence. |
| **A2A task delegation doesn't complete in demo time** | 🟡 Medium | Keep demo tasks simple (upload small file, query balance). Pre-seed wallet accounts. |
| **Validation bot rejects for missing file** | 🟡 Medium | Cross-reference all 4 prior rejection reasons (C23-C33). Build a pre-submission checklist script. |
| **3 simultaneous agent instances conflict** | 🟢 Low | Use different content topics, different wallet accounts, different instancePersistencePath. |
| **Disk space (49GB free)** | 🟢 Low | Nix build of agent module is small. LEZ + Docker images already pulled. Monitor during builds. |

---

## 5. Validation Gates (Go/No-Go Before Each Phase)

**Gate 0 (after Phase 0):** Module builds and loads into logoscore. Cross-module call to LEZ wallet works.  
→ If FAIL: debug module system, don't proceed until resolved.

**Gate 1 (after Phase 2):** Can query wallet balance and send a transaction from the agent module.  
→ If FAIL: LEZ wallet FFI integration issue. Debug wallet_ffi.h bindings.

**Gate 3 (after Phase 3):** Can upload a file to Storage and send a message via Delivery.  
→ If FAIL: check if storage/delivery nodes are running and reachable.

**Gate 4 (after Phase 4):** Spending gate blocks over-threshold transactions and routes approval via owner channel.  
→ If FAIL: verify delivery module connectivity between agent and owner terminal.

**Gate 5 (after Phase 5):** Two agents discover each other and complete a delegated task.  
→ If FAIL: verify discovery topic subscription + message routing.

**Gate 7 (after Phase 7):** Demo script runs end-to-end without errors. CI is green.  
→ If FAIL: fix failing step, re-run from top.

**Gate 8 (after Phase 8):** Video recorded, all C1-C33 checklist items verified.  
→ If FAIL: don't submit. Fix and re-record.

---

## 6. Key Technical Reference: Universal Module Authoring

From `logos-module-builder` skill docs — the fastest path:

```bash
# 1. Scaffold from template
mkdir logos-autonomous-agent-module && cd $_
nix flake init -t github:logos-co/logos-module-builder

# 2. Edit metadata.json — set dependencies, interface: universal
# 3. Write src/agent_impl.h + src/agent_impl.cpp (derive LogosModuleContext)
# 4. Build
git init && git add -A
nix build

# 5. Inspect generated methods
lm methods ./result/lib/agent_module_plugin.so --json

# 6. Run
logoscore -m ./result/lib -l agent_module -c "agent_module.status()"
```

### Cross-Module Call Pattern (from skill docs)

```cpp
// src/agent_impl.cpp
#include "logos_sdk.h"  // Generated at build time — defines LogosModules

std::string AgentImpl::getChainBalance(const std::string& account_hex, bool is_public)
{
    auto& lez = modules().logos_execution_zone;
    std::string balance = lez.get_balance(account_hex, is_public);
    return balance;
}
```

### Event Declaration Pattern

```cpp
// src/agent_impl.h
class AgentImpl : public LogosModuleContext
{
public:
    std::string dispatchSkill(const std::string& skill_name, const std::string& args_json);

logos_events:
    void skillExecuted(const std::string& skill_name, const std::string& result);
    void approvalRequested(const std::string& task_id, const std::string& skill, const std::string& amount);
    void approvalReceived(const std::string& task_id, bool approved);
    void agentDiscovered(const std::string& agent_card_json);
    void taskStatusChanged(const std::string& task_id, const std::string& state, const std::string& result);
};
```

---

## 7. Repo Structure (Target)

```
logos-autonomous-agent-module/
├── README.md
├── LICENSE                           # MIT + Apache 2.0 dual
├── metadata.json                     # Module manifest (the "module.json")
├── flake.nix                         # Nix flake — mkLogosModule
├── CMakeLists.txt                    # LogosModule.cmake integration
│
├── src/
│   ├── agent_impl.h                  # Universal impl class
│   ├── agent_impl.cpp                # Main impl — dispatch, lifecycle
│   ├── skill_registry.h/.cpp         # Skill table
│   ├── spending_gate.h/.cpp          # Threshold + approval
│   ├── owner_channel.h/.cpp          # Owner messaging
│   ├── a2a_protocol.h/.cpp           # A2A discovery + task lifecycle
│   ├── agent_config.h                # Config struct
│   ├── json_utils.h/.cpp             # JSON helpers
│   └── skills/
│       ├── storage_skills.h/.cpp
│       ├── messaging_skills.h/.cpp
│       ├── chain_skills.h/.cpp
│       ├── a2a_skills.h/.cpp
│       └── meta_skills.h/.cpp
│
├── configs/
│   ├── alpha-storage.json            # Agent Alpha config
│   ├── beta-messaging.json           # Agent Beta config
│   └── gamma-blockchain.json         # Agent Gamma config
│
├── tests/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── mocks/                        # Mock module dependencies
│   ├── test_skill_registry.cpp
│   ├── test_spending_gate.cpp
│   ├── test_a2a_protocol.cpp
│   └── test_chain_skills.cpp
│
├── scripts/
│   ├── demo.sh                       # End-to-end demo script
│   └── narrate-demo.sh               # TTS narration companion
│
├── docs/
│   └── ARCHITECTURE.md               # Architecture decisions + diagrams
│
└── .github/
    └── workflows/
        └── ci.yml                    # Build + test pipeline
```

---

## 8. What We Learned From Prior Attempts

| Attempt | Key Failure | Our Fix |
|---------|-------------|---------|
| Beach-Bum #34 | Built standalone daemon (Ollama + Waku), not a Logos Core module | We build a proper universal module with `metadata.json` + `flake.nix` |
| retraca #81 | Missing: Success Criteria Checklist, FURPS, T&C, LICENSE, CI, demo.sh | Pre-submission checklist script verifies all C23-C33 |
| retraca #85 | Missing: only video demo + module.json | `module.json` = our `metadata.json` at repo root; video is Phase 8 |
| retraca #88 | Missing: ONLY the narrated video demo | Phase 8 dedicated to recording; don't submit without it |

**The pattern is clear:** the code wasn't the blocker. It was the submission packaging. We front-load all submission requirements as checklist items C23-C33 and verify each before opening the PR.
