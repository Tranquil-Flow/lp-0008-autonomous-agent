# LP-0008: Autonomous AI Agent Module for Logos Core

An autonomous AI agent module for the Logos blockchain ecosystem. The agent holds its own shielded LEZ wallet, stores and retrieves files via Logos Storage, communicates over Logos Messaging, coordinates with other agents via the A2A protocol, and enforces owner-configured spending thresholds.


## Current submission-readiness status

This repository is under active strict LP-0008 completion work. CI and local validators prove repository hygiene, module buildability, raw C ABI behavior, Logos Core co-load, and wallet/storage/messaging primitives. They do **not** by themselves prove final Lambda Prize acceptance.

Before final submission, the remaining strict gaps in `docs/submission-readiness-matrix.md` must be closed or explicitly accepted as residual risk: Basecamp owner-channel interaction, task-linked live LEZ payment between separate agents, three separate testnet agents/use cases, CU documentation, and proof-mode/video evidence. A2A task envelopes now have live Logos Messaging transport evidence in the deep verifier, and the generic smoke demo proves the per-task payment hook deterministically.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Logos Core Runtime                  │
│  ┌─────────────┐  ┌───────────┐  ┌───────────────┐  │
│  │ LEZ Wallet  │  │  Storage  │  │   Messaging    │  │
│  └──────┬──────┘  └─────┬─────┘  └───────┬───────┘  │
│         │                │                 │          │
│  ┌──────┴────────────────┴─────────────────┴───────┐ │
│  │              Agent Module (this)                │ │
│  │  ┌──────────────┐  ┌──────────────┐            │ │
│  │  │ SkillRegistry│  │ SpendingGate │            │ │
│  │  │ (27 registered)│ │ (thresholds) │            │ │
│  │  └──────────────┘  └──────────────┘            │ │
│  │  ┌──────────────┐  ┌──────────────┐            │ │
│  │  │ AgentConfig  │  │ Persistence  │            │ │
│  │  │  (runtime)   │  │  (file JSON) │            │ │
│  │  └──────────────┘  └──────────────┘            │ │
│  └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

### Skill Categories

| Category   | Skills                                                                 |
|------------|------------------------------------------------------------------------|
| **Meta**   | `meta.skills`, `meta.status`, `meta.configure`                        |
| **Storage**| `storage.upload`, `storage.download`, `storage.list`, `storage.share` |
| **Messaging** | `messaging.send`, `messaging.join`, `messaging.create_group`       |
| **Wallet** | `wallet.balance`, `wallet.send`, `wallet.history`                     |
| **Program**| `program.query`, `program.call`, `program.deploy`                     |

### Live integration boundaries

- Wallet balance/history and funded sends use the LEZ wallet FFI when a funded rc3 wallet is mounted; `scripts/run_live_wallet_send_verify.py` is the opt-in public-testnet proof.
- Program calls/deploys fail closed inside the module because Logos Core exposes no module-safe LEZ program SDK/C ABI yet; the compatible rc3 SPEL/lgs external path is documented in `docs/upstream/program-live-api.md`.

| **Agent**  | `agent.card`, `agent.discover`, `agent.task`, `agent.receive`, `agent.subscribe`, `agent.cancel`, `agent.complete` |

### Spending Threshold

The owner configures:
- `per_tx_limit` — max tokens per single transaction (default: 1000)
- `per_period_limit` — max tokens per 24h window (default: 10000)
- `period_seconds` — rolling window duration (default: 86400)

Below threshold → autonomous execution. Above threshold → `owner_approval_required` response.

### Persistence

State is stored as JSON files under `$LOGOS_AGENT_STATE_DIR` (or `~/.local/share/logos-agent/`):
- `storage.json` — file index with addresses, labels, sizes
- `spend_history.json` — rolling spend records for threshold enforcement

## Build

```bash
nix build .#install
```

Module artifact: `result/modules/agent_module/agent_module_plugin.dylib` on Darwin/macOS or `result/modules/agent_module/agent_module_plugin.so` on Linux.

## Demo

```bash
./demo.sh
```

The demo:
1. Builds the module via Nix
2. Compiles a standalone C ABI caller (`tests/cabi_call.cpp`)
3. Exercises the core default skill surface via `dispatchSkill()` through the raw C ABI
4. Asserts correct return values for each skill
5. Proves the spending gate blocks over-threshold transactions

## Testing

```bash
# C ABI verification harness
tests/cabi_call.cpp     # dlopen + logos_module_dispatch caller
tests/assert_result.py  # smart JSON unwrap + field assertion helper
```


## Logos Core Integration Smoke Test

`demo.sh` validates the C ABI in-process. For real Logos Core runtime coverage against sibling modules, use:

```bash
scripts/run_logoscore_integration.sh all
```

The integration harness builds missing `.#install` outputs, starts a temporary `logoscore` daemon, and verifies:

1. Stage B: `agent_module + storage_module` live `storage.upload` and registry-backed `storage.list`.
2. Stage C: `agent_module + storage_module + delivery_module` live storage plus live `messaging.send` to a valid LIP-23 content topic.
3. Safety: malformed delivery topics are kept in the local simulated log so `delivery_module` is not crashed by invalid input.

For the three-agent A2A control-plane demo, run:

```bash
scripts/run_multi_agent_a2a_demo.sh
```

That harness configures Alpha/Beta/Gamma identities, publishes their Agent Cards, verifies discovery returns all three, and exercises a delegated task lifecycle (`working -> subscribed -> cancelled`).

### Basecamp readiness

The repository includes `scaffold.toml` with `[modules.agent_module] = path:.#lgx`, so reviewers can inspect or prepare a Basecamp install path with `lgs basecamp modules --show`. The `.lgx` artifact is verified with:

```bash
nix build .#lgx-portable --out-link result-lgx-portable
```

This is a compatibility/evidence path, not a GUI claim: the final submission video should not claim Basecamp GUI interaction unless a real `lgs basecamp install/launch` session is recorded separately.

Defaults match the M4 Pro Basecamp workspace. Override paths when needed:

```bash
LOGOSCORE_CLI=/path/to/logoscore \
LOGOS_BASECAMP_ROOT=/path/to/logos-basecamp \\
LP0008_TEST_ROOT=$HOME/lp0008-phase0 \
scripts/run_logoscore_integration.sh stage-c
```

## Strict default-skill matrix

`scripts/run_strict_skill_matrix.py` is the strict matrix gate for the registered default skill surface. It builds the agent, storage, and delivery modules with Nix, co-loads them in `logoscore`, then verifies 27 registered skills: the original 23 spec skills plus approval and A2A lifecycle helpers. The matrix includes success/bounded behavior, fail-closed behavior where applicable, explicit storage share and messaging group semantics, malformed A2A envelope handling, six concurrent live Logos Messaging sends, and a final module-loaded resilience check. The generated evidence is documented in `docs/strict-default-skill-matrix.md`.

## Final strict evidence gate

`scripts/run_final_strict_evidence.sh` is the umbrella non-video evidence gate. It writes a SHA-pinned JSON summary plus per-step logs under `~/lp0008-phase0/final_strict_evidence_<timestamp>/`, runs all strict phase scripts, and records known upstream/tooling blockers as `blocked` rather than silently passing them. A manual-only opt-in GitHub workflow is available at `docs/live-strict-evidence.workflow.yml (copy to .github/workflows/live-strict-evidence.yml with workflow-scope token)`.

## Final pre-video evidence gate

Before recording the narrated Lambda Prize video, run the strict umbrella gate:

```bash
scripts/run_final_strict_evidence.sh
```

This runs the non-video proof bundle: Nix build, raw C ABI demo, Logos Core storage/delivery co-load, three-agent identity/use-case evidence, A2A schema and transport checks, readiness validators, and explicit blocker capture for upstream wallet/program API limits. The legacy `scripts/run_final_pre_video_evidence.sh` remains nested inside the strict gate unless `LP0008_SKIP_PREVIDEO_BUNDLE=1` is set.

See `docs/strict-success-criteria-evidence.md` for the exact success-criteria mapping.

## Deployment

```bash
# Load agent module in Logos Core
logoscore -m ./result/modules -l agent_module -c 'agent_module.dispatchSkill("agent.card","[]")' --quit-on-finish
```

## Skill Interface

New skills are added via the `SkillRegistry::addMeta()` method:

```cpp
reg.addMeta(
    "custom.skill",           // dotted name
    "custom",                  // category
    "Description",             // human-readable
    R"({"param":"value"})",   // input schema (JSON example)
    R"({"result":"..."})"      // output schema (JSON example)
);
```

Then add a handler in `dispatchSkill()`:

```cpp
if (skill_name == "custom.skill") return customSkill(s(0));
```

## License

MIT — see [LICENSE](LICENSE).

- Wallet approvals: above-threshold sends create persisted owner approvals with retry/reject/approve commands.
