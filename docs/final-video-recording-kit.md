# LP-0008 final video recording kit

This kit records a terminal-only LP-0008 demo from the M4 Pro. It does not push to `logos-co/lambda-prize` and does not open any PR.

## Claim boundary

Show these claims only:

- The agent module builds and loads through Logos Core/C ABI paths.
- The raw harness verifies the skill surface.
- Storage and delivery live paths are exercised where module APIs exist.
- A2A task transport is shown over live Logos Messaging in `scripts/run_lp0008_deep_verify.py` and `scripts/run_final_strict_evidence.sh`; paid-A2A LEZ transfer remains explicitly blocked by the rc3 wallet transfer proof panic.
- Three configured agent profiles/cards/topics and three illustrative use cases are shown by executable scripts.
- Approval persistence, timeout guard behavior, and post-failure skill isolation are shown by `scripts/run_resilience_evidence.sh`.
- Wallet balance/history and funded wallet-send use live LEZ wallet FFI against public testnet when a funded rc3 wallet is mounted.
- Program query is simulated; `program.call` and `program.deploy` return bounded errors because Logos Core exposes no module-safe LEZ program SDK/C ABI yet.
- Basecamp CLI artifact readiness may be shown by displaying `scaffold.toml`, `lgs basecamp modules --show`, and `nix build .#lgx`; this is artifact readiness, not GUI interaction proof.

Do not claim GUI/Basecamp owner-channel interaction or live program execution unless those are recorded separately with real evidence.

## Dry-run transcript hygiene

Before recording, run this dry-run hygiene check:

```bash
ssh m4pro 'cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent && RUN_COMMANDS=0 SECTION_PAUSE=0 COMMAND_PAUSE=0 bash scripts/record_final_video.sh > /tmp/lp0008-recording-dry-run.txt && grep -Ei "do not cite|supersedes|pre-fix|remaining work|URL is inserted|Evi-gated|reviewer|closed|denied|denial|overclaim|resubmission once|fresh video|old PR|stale|this video URL|missing narrated video" /tmp/lp0008-recording-dry-run.txt || true && sed -n "1,220p" /tmp/lp0008-recording-dry-run.txt'
```

The grep should print no matches. If it prints a match, edit the recording script before recording.

## Recording command

This command now runs `scripts/record_final_video.sh`, whose first substantive proof scene runs `scripts/run_final_strict_evidence.sh`. Expected strict-gate status before upstream wallet/program fixes is `ok_with_blockers`: no failed shell steps, one retained paid-A2A wallet blocker.

Run this from the laptop terminal while recording the terminal window:

```bash
ssh -t m4pro 'bash -lc '''
set -euo pipefail
PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
bash scripts/record_final_video.sh
''''
```

## Optional live wallet clip

If you want to spend public-testnet LEZ during recording, run this as a short second clip or after the main recording:

```bash
ssh -t m4pro 'bash -lc '''
set -euo pipefail
PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
LP0008_LIVE_WALLET_HOME=~/lp0008-phase0/rc3_faucet_wallet LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq LP0008_LIVE_SEND_RECIPIENT=yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3 LP0008_LIVE_WALLET_AMOUNT=1 ./scripts/run_live_wallet_send_verify.py
''''
```

If the optional wallet proof is too slow, cite the retained public-testnet transaction only in docs/PR text and keep the video focused on the reproducible evidence bundle.

## Narration beats

1. "This is LP-0008, an autonomous AI agent module for Logos Core."
2. "The demo starts from the public implementation repository and exact commit."
3. "The build uses the Nix/module-builder path and produces the agent module artifact."
4. "Basecamp artifact readiness is shown at the CLI/package level; this terminal recording does not claim GUI interaction."
5. "The core demo calls the raw C ABI harness."
6. "Integration loads agent, storage, and delivery together through Logos Core."
7. "Deep verification shows live Logos Messaging transport for A2A task envelopes."
8. "The use-case proof shows a personal file vault, marketplace payment hook, and multi-agent workflow."
9. "The resilience proof shows pending approvals persist, expired approvals return bounded errors, and a failed task does not poison later skill execution."
10. "Program execution is not faked: call and deploy return bounded errors until Logos Core exposes a module-safe LEZ program SDK/C ABI."

## After upload

1. Copy the final public/accessible video URL.
2. Patch `SUBMISSION.md` in this repo.
3. Rerun:

```bash
./scripts/validate_acceptance_readiness.py
./scripts/run_final_strict_evidence.sh
```

4. Commit and push the video URL update to `Tranquil-Flow/lp-0008-autonomous-agent`.
5. Only after explicit approval, prepare a minimal `solutions/LP-0008.md` branch for `logos-co/lambda-prize`.

## Direct-on-M4 alternative

If you are already in a shell on the M4 Pro, run:

```bash
PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
bash scripts/record_final_video.sh
```
