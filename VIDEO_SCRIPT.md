# Narrated Demo Video Script — LP-0008

## Setup (30 sec)

> "I'm demonstrating LP-0008, an autonomous AI agent module for Logos Core.
> This module implements 21 skills across six categories: meta, storage,
> messaging, wallet, program, and agent-to-agent coordination via the A2A protocol."

[Show: terminal open in project directory]

## Architecture Walkthrough (60 sec)

> "The module is built with the Logos module-builder toolchain and packaged
> as a dynamically loadable .lgx file. Let me show the architecture."

[Show: `cat README.md` — scroll to architecture diagram]

> "Key design decisions:
> - A single AgentModuleImpl class with 21 methods, registered in a SkillRegistry
> - A SpendingGate that enforces per-transaction and per-period limits
> - File-backed JSON persistence under the agent's state directory
> - An A2A-compatible Agent Card with payment, skills, and authentication"

## Build (30 sec)

[Run: `nix build .#install --print-build-logs`]

> "The module builds with a single Nix command. It produces an installable
> .lgx package that Logos Core loads at runtime."

## Demo Run (90 sec)

[Run: `./demo.sh`]

> "The demo script exercises all 21 skills through the raw C ABI —
> we use a direct dlopen caller because the logoscore CLI has a display
> quirk with JSON returns."

[Narrate key sections as they pass:]

**META section:**
> "Greet returns our identity. Meta.skills lists all 21 capabilities with
> input and output schemas. Meta.status shows the agent is running with
> default spending thresholds: 1000 per transaction, 10000 per 24 hours."

**STORAGE section:**
> "Storage.upload takes a local file, encrypts it, and returns a content
> address. Storage.download retrieves it. Storage.list shows our file index.
> This is real file-backed persistence — the index survives across restarts."

**WALLET section — spending gate:**
> "This is the key security mechanism. A small transfer of 10 units is
> approved automatically — it's within the per-tx limit. But a large
> transfer of 390 million units is blocked: the spending gate returns
> 'exceeds_per_tx_limit' and flags it for owner approval."

**AGENT section — A2A:**
> "Agent.card returns a fully A2A-compatible Agent Card: protocol a2a,
> version 1.0, with 21 skills and LEZ payment. Agent.task sends a task
> request following the A2A lifecycle. Agent.subscribe gets streaming status.
> Agent.cancel cancels and triggers refund."

## Closing (30 sec)

> "All 21 skills verified. The module is open source under MIT.
> The spending gate enforces owner-configured limits. The A2A Agent Card
> follows the industry standard for agent interoperability, extended with
> Logos-native payment and privacy."

---

**Recording tips:**
- Use `asciinema rec` or QuickTime screen recording
- Keep terminal font large enough to read (18pt+)
- Record in a quiet room with external mic
- Total length: ~4 minutes
- Export as MP4, upload to YouTube unlisted or Google Drive
