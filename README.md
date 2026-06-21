# LP-0008: Autonomous AI Agent Module for Logos Core

An autonomous AI agent module for the Logos blockchain ecosystem. The agent holds its own shielded LEZ wallet, stores and retrieves files via Logos Storage, communicates over Logos Messaging, coordinates with other agents via the A2A protocol, and enforces owner-configured spending thresholds.

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
│  │  │  (23 skills) │  │ (thresholds) │            │ │
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
3. Exercises all 23 skills via `dispatchSkill()` through the raw C ABI
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

Defaults match the M4 Pro Basecamp workspace. Override paths when needed:

```bash
LOGOSCORE_CLI=/path/to/logoscore \
LOGOS_BASECAMP_ROOT=$HOME/Projects/logos-basecamp \
LP0008_TEST_ROOT=$HOME/lp0008-phase0 \
scripts/run_logoscore_integration.sh stage-c
```

## Final pre-video evidence gate

Before recording the narrated Lambda Prize video, run:

```bash
scripts/run_final_pre_video_evidence.sh
```

This runs every non-video proof: Nix build, raw C ABI all-skill demo, Logos Core storage/delivery co-load, three-agent A2A lifecycle proof, readiness validator, and the optional live LEZ wallet send verifier when funded-wallet environment variables are set.

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
