# LP-0008 Final Demo Video Script

Target length: 6-8 minutes. Terminal-only recording. Use the exact command block in `docs/final-video-recording-kit.md` or run `scripts/run_final_pre_video_evidence.sh` directly on the M4 Pro.

## Claims to make

- LP-0008 is implemented as a Logos Core autonomous agent module.
- The public repo and commit shown in the recording are the submitted artifacts.
- All 23 skills are verified through raw C ABI / Logos Core paths.
- Storage and delivery are exercised through live Logos Core modules where APIs exist.
- Three configured agents prove A2A Agent Cards, discovery, task intake, subscribe readback, and terminal cancel guard.
- LEZ wallet send is real public-testnet evidence when the funded rc3 wallet is mounted.
- Program execution is not faked: program call/deploy fail closed until Logos exposes a module-safe LEZ program SDK/C ABI.

## Claims not to make

- Do not claim GUI/Basecamp evidence.
- Do not claim live LEZ program call/deploy from inside the module.
- Do not claim synchronous delivery receive/poll exists; say `agent.receive` uses persisted inbox polling because delivery currently exposes events but no module-callable poll API.

## Scene 1 — repo and commit, 30 sec

On screen:

```bash
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
git status -sb
git log -1 --oneline
git ls-remote origin refs/heads/main refs/heads/feat/real-lez-wallet
```

Narration cue:

"This is the LP-0008 autonomous AI agent module for Logos Core. I am starting from the public implementation repo and showing the exact commit being submitted. Both main and the feature branch point to this commit."

## Scene 2 — build, 45 sec

On screen:

```bash
nix build .#install --out-link result
find result/modules -maxdepth 2 -type f | sort
```

Narration cue:

"The module builds through Nix and the Logos module-builder path. The result contains the loadable agent module artifact used by Logos Core and by the raw C ABI harness."

## Scene 3 — core all-skill demo, 90 sec

On screen:

```bash
bash demo.sh
```

Narration cue:

"The core demo calls the raw module dispatch path through a direct dlopen C ABI caller. This avoids the known logoscore display quirk for JSON returns and verifies actual returned payloads. It exercises all 23 skills: meta, storage, messaging, wallet, program, and agent coordination."

Callouts while it runs:

- "Meta.skills lists all 23 skills with schemas."
- "The spending gate approves below-threshold sends and blocks over-threshold sends with `exceeds_per_tx_limit`."
- "A2A card returns protocol `a2a`, version `1.0`, payment metadata, and skill schema."
- "Program call and deploy fail closed; they are not simulated as success."

## Scene 4 — Logos Core integration, 90 sec

On screen:

```bash
./scripts/run_logoscore_integration.sh
```

Narration cue:

"This starts fresh Logos Core daemon instances and co-loads the agent with storage and delivery modules. Stage B proves live storage upload/list through storage_module. Stage C proves agent, storage, and delivery together, including a valid LIP-23 delivery send. The invalid-topic guard shows the agent prevents a known delivery abort path and keeps the daemon alive."

## Scene 5 — three-agent A2A proof, 75 sec

On screen:

```bash
./scripts/run_multi_agent_a2a_demo.sh
```

Narration cue:

"This configures three separate agents: Alpha Storage, Beta Messaging, and Gamma Chain. Each has its own Agent Card, task topic, owner topic, and role. The demo proves discovery of all three cards, inbound task envelope processing through `agent.receive`, queued-to-working-to-completed lifecycle, subscribe readback, and terminal cancel guards."

## Scene 6 — deep verifier and readiness gate, 60 sec

On screen:

```bash
./scripts/run_lp0008_deep_verify.py
./scripts/validate_acceptance_readiness.py
```

Narration cue:

"The deep verifier checks the full skill surface and important invariants. The readiness validator checks the required docs, claim boundaries, dependency policy, upstream gap documents, and final evidence map."

## Scene 7 — final pre-video gate, 90 sec

Preferred on screen:

```bash
LP0008_LIVE_WALLET_HOME=~/lp0008-phase0/rc3_faucet_wallet LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq LP0008_LIVE_SEND_RECIPIENT=yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3 LP0008_LIVE_WALLET_AMOUNT=1 ./scripts/run_final_pre_video_evidence.sh
```

If live wallet send is too slow for the recording, show the retained successful log instead:

```bash
tail -80 ~/lp0008-phase0/final_pre_video_evidence.log
```

Narration cue:

"This is the final pre-video evidence gate. It runs every non-video proof and, with the funded rc3 wallet mounted, submits a real public-testnet wallet send through the module FFI. The latest retained run completed with `PRE_VIDEO_EVIDENCE_OK`, transaction `5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3`, `tx_found: true`, and observed balance `149 -> 144`."

## Scene 8 — close, 30 sec

On screen:

```bash
sed -n '1,140p' docs/strict-success-criteria-evidence.md
```

Narration cue:

"The evidence map links each LP-0008 success criterion to a command or artifact. The only remaining publication step after this recording is to paste the video URL into `SUBMISSION.md` and into the Lambda Prize `solutions/LP-0008.md` PR."

## Upload checklist

1. Export MP4 at readable terminal resolution.
2. Upload unlisted/public to a reviewer-accessible URL.
3. Patch video URL into `SUBMISSION.md`.
4. Re-run:

```bash
./scripts/validate_acceptance_readiness.py
```

5. Commit/push video URL update.
6. Prepare `solutions/LP-0008.md` from the updated `SUBMISSION.md`.
