# LP-0008 final submission preflight

State: ready except narrated video URL.

## What is complete

- Implementation repo default branch is the reviewer clone target.
- `scripts/run_final_strict_evidence.sh` is the primary umbrella non-video gate and emits `FINAL_STRICT_EVIDENCE_COMPLETE` with `ok` or honest `ok_with_blockers` status. The older `scripts/run_final_pre_video_evidence.sh` remains part of that strict gate and emits `PRE_VIDEO_EVIDENCE_OK`.
- `scripts/record_final_video.sh` is the recording script.
- `docs/final-video-audio-narration.md` follows the recording script section order.
- `SUBMISSION.md` has exactly one publication placeholder: `PENDING_VIDEO_URL`.
- Lambda Prize PR must not be opened until the video URL is inserted and explicit approval is given.
- `scripts/check_lp0008_submission_window.py` confirms there is no currently open LP-0008 Lambda Prize PR and that the same-builder cadence window is open. Current observed prior LP-0008 PRs are closed: #34 (Beach-Bum), #81/#85/#88 (retraca); none are from Tranquil-Flow.
- `module.json` is present at repository root for the Lambda Prize/Basecamp mini-app validator and declares `logos.messaging`/A2A capabilities while preserving honest GUI proof boundaries.

## Record

From the laptop terminal:

```bash
ssh -t m4pro 'bash -lc '''
set -euo pipefail
PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
bash scripts/record_final_video.sh
''''
```

## Patch after upload

```bash
ssh m4pro 'bash -lc '''
set -euo pipefail
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent
LAMBDA_PRIZE_WORKTREE=$HOME/lp0008-phase0/lambda-prize-local   scripts/apply_final_video_url.py "VIDEO_URL_HERE"
python3 scripts/validate_acceptance_readiness.py
git diff --check
git status --short
''''
```

Then commit/push the implementation repo video URL patch, run CI on the exact SHA, validate the Lambda Prize local draft, and ask for explicit approval before opening or updating any public Lambda Prize PR.

## Claims to keep bounded

Claim:

- loadable Logos Core module and C ABI dispatch verification;
- live storage/delivery co-load where APIs exist;
- live Logos Messaging A2A transport;
- three configured agents and three illustrative use-case evidence;
- owner approval persistence, timeout guard, and skill failure isolation;
- retained public-testnet wallet send evidence.

Do not claim:

- separate Basecamp GUI owner-channel proof;
- live inter-agent LEZ payment tied to task acceptance;
- live `program.call`/`program.deploy` execution;
- RISC0 proof-mode program execution;
- acceptance guaranteed by validators.


## Final strict gate

Run `scripts/run_final_strict_evidence.sh` before recording the final video, and note that `scripts/record_final_video.sh` now runs it as the first substantive evidence scene. Current expected status before the upstream wallet/program APIs are fixed is `ok_with_blockers`, with no failed shell steps and explicit blocked evidence for rc3 wallet transfer proving.
