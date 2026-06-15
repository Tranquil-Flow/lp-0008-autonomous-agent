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
│  │  │  (21 skills) │  │ (thresholds) │            │ │
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
| **Agent**  | `agent.card`, `agent.discover`, `agent.task`, `agent.subscribe`, `agent.cancel` |

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

Module artifact: `result/modules/agent_module/agent_module_plugin.dylib`

## Demo

```bash
./demo.sh
```

The demo:
1. Builds the module via Nix
2. Compiles a standalone C ABI caller (`tests/cabi_call.cpp`)
3. Exercises all 21 skills via `dispatchSkill()` through the raw C ABI
4. Asserts correct return values for each skill
5. Proves the spending gate blocks over-threshold transactions

## Testing

```bash
# C ABI verification harness
tests/cabi_call.cpp     # dlopen + logos_module_dispatch caller
tests/assert_result.py  # smart JSON unwrap + field assertion helper
```

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
