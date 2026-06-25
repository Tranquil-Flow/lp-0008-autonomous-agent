# LP-0008 Final Demo Video Script

Target length: 8-12 minutes. Terminal-only recording. Record from this computer/laptop terminal, but execute the demo on the M4 Pro. The safest path is the SSH command block in `docs/final-video-recording-kit.md`; that command is launched locally and runs every proof on `m4pro`. If you are already logged into the M4 Pro, run `scripts/record_final_video.sh` directly there. The first substantive proof scene is the strict umbrella gate, `scripts/run_final_strict_evidence.sh`, which records pass/blocker evidence instead of hiding upstream limitations.

## Claims to make

- LP-0008 is implemented as a Logos Core autonomous agent module.
- The public repo and commit shown in the recording are the submitted artifacts.
- The skill surface is verified through raw C ABI / Logos Core paths.
- Storage and delivery are exercised through live Logos Core modules where APIs exist.
- Three funded/persisted agent identities and three illustrative use cases prove A2A Agent Cards, discovery, task intake, subscribe readback, use-case execution, and terminal cancel guard.
- Standalone LEZ wallet-send evidence is real public-testnet evidence when the funded rc3 wallet is mounted; paid A2A task payment remains explicitly blocked by the rc3 wallet transfer proof panic after live Logos Messaging task completion.
- Program execution is not faked: program call/deploy return bounded errors until Logos exposes a module-safe LEZ program SDK/C ABI.

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

## Scene 2 — final strict evidence umbrella, 3-5 min

On screen:

```bash
./scripts/run_final_strict_evidence.sh
```

Narration cue:

"This is the strict non-video evidence gate. It runs the build, raw C ABI demo, strict live preflight, three-agent identity proof, paid-A2A evidence, Basecamp owner approval evidence, program/CU boundary evidence, strict default-skill matrix, and readiness validator. The expected status today is `ok_with_blockers`: no shell failures, with the paid-A2A LEZ transfer explicitly recorded as blocked by the upstream rc3 wallet transfer proof panic after the live Logos Messaging task succeeds."

## Scene 3 — build, 45 sec

On screen:

```bash
nix build .#install --out-link result
find result/modules -maxdepth 2 -type f | sort
```

Narration cue:

"The module builds through Nix and the Logos module-builder path. The result contains the loadable agent module artifact used by Logos Core and by the raw C ABI harness."

## Optional Scene 3b — Basecamp CLI readiness, 30 sec

Include this if you want to show Basecamp preparedness without claiming GUI interaction.

On screen:

```bash
sed -n '1,80p' scaffold.toml
lgs basecamp modules --show
nix build .#lgx-portable --out-link result-lgx-portable
```

Narration cue:

"The project is also Basecamp-ready at the CLI artifact level: `scaffold.toml` captures `agent_module` as `path:.#lgx`, and the evaluator-loadable `.#lgx-portable` artifact builds. I am not claiming a GUI/Basecamp interaction here; the tested runtime evidence remains the Logos Core and C ABI paths shown next."

## Scene 4 — core all-skill demo, 90 sec

On screen:

```bash
bash demo.sh
```

Narration cue:

"The core demo calls the raw module dispatch path through a direct dlopen C ABI caller. This avoids the known logoscore display quirk for JSON returns and verifies actual returned payloads. It exercises the default skill surface: meta, storage, messaging, wallet, program, and agent coordination, with approval and A2A lifecycle helpers visible in `meta.skills`."

Callouts while it runs:

- "Meta.skills lists all 23 skills with schemas."
- "The spending gate approves below-threshold sends and blocks over-threshold sends with `exceeds_per_tx_limit`."
- "A2A card returns protocol `a2a`, version `1.0`, payment metadata, and skill schema."
- "Program call and deploy return bounded errors; they are not simulated as success."

## Scene 5 — Logos Core integration, 90 sec

On screen:

```bash
./scripts/run_logoscore_integration.sh
```

Narration cue:

"This starts fresh Logos Core daemon instances and co-loads the agent with storage and delivery modules. Stage B proves live storage upload/list through storage_module. Stage C proves agent, storage, and delivery together, including a valid LIP-23 delivery send. The invalid-topic guard shows the agent prevents a known delivery abort path and keeps the daemon alive."

## Scene 6 — three-agent A2A and use-case proof, 90 sec

On screen:

```bash
./scripts/run_multi_agent_a2a_demo.sh
./scripts/run_three_use_cases_demo.sh
./scripts/run_resilience_evidence.sh
```

Narration cue:

"This configures Alpha Storage, Beta Messaging, and Gamma Chain with separate Agent Cards, task topics, owner topics, and roles. The use-case script proves personal file vault, marketplace payment hook, and multi-agent workflow evidence. The resilience script proves persisted approvals survive a process boundary, expired approvals return bounded errors, and a failed task does not poison later skill execution."

## Scene 7 — deep verifier and readiness gate, 60 sec

On screen:

```bash
./scripts/run_lp0008_deep_verify.py
./scripts/validate_acceptance_readiness.py
```

Narration cue:

"The deep verifier checks the full skill surface and important invariants. The readiness validator checks the required docs, claim boundaries, dependency policy, upstream gap documents, and final evidence map."

## Optional Scene 8 — standalone wallet proof / legacy pre-video gate, 90 sec

Preferred on screen:

```bash
LP0008_LIVE_WALLET_HOME=~/lp0008-phase0/rc3_faucet_wallet LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq LP0008_LIVE_SEND_RECIPIENT=yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3 LP0008_LIVE_WALLET_AMOUNT=1 ./scripts/run_final_pre_video_evidence.sh
```

If live wallet send is too slow for the recording, show the retained successful log instead:

```bash
tail -80 ~/lp0008-phase0/final_pre_video_evidence.log
```

Narration cue:

"This optional clip shows the standalone public-testnet wallet proof or the legacy pre-video evidence gate. The strict gate shown earlier is the authoritative non-video umbrella for current submission readiness; this wallet clip is standalone wallet-send evidence, not a paid-A2A task-payment claim. The latest retained run completed with `PRE_VIDEO_EVIDENCE_OK`, transaction `5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3`, `tx_found: true`, and observed balance `149 -> 144`."

## Scene 9 — close, 30 sec

On screen:

```bash
sed -n '1,140p' docs/strict-success-criteria-evidence.md
```

Narration cue:

"The evidence map links each LP-0008 success criterion to a command or artifact. After recording, paste the video URL into `SUBMISSION.md` and the Lambda Prize `solutions/LP-0008.md` draft, rerun validators, and ask for explicit approval before opening a public prize PR."

## Upload checklist

1. Export MP4 at readable terminal resolution.
2. Upload unlisted/public to a accessible URL.
3. Patch video URL into `SUBMISSION.md`.
4. Re-run:

```bash
./scripts/validate_acceptance_readiness.py
```

5. Commit/push video URL update.
6. Prepare `solutions/LP-0008.md` from the updated `SUBMISSION.md`.
