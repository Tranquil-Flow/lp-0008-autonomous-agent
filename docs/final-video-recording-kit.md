# LP-0008 final video recording kit

This is a public, reviewer-facing recording checklist. It avoids host-specific paths: run commands from a clean checkout on the machine that has the required Logos/Nix/wallet prerequisites.

## Recording command

```bash
SECTION_PAUSE=2 COMMAND_PAUSE=1 SCENE_PAUSE=3 bash scripts/record_final_video.sh
```

The first substantive proof scene runs `scripts/run_final_strict_evidence.sh`. Expected strict-gate status before upstream wallet/program fixes is `ok_with_blockers`: no hidden failed shell steps, and explicit blocked evidence for paid-A2A wallet transfer proving or program/CU APIs when those remain unavailable.

## What the video must show

- Repository, commit, and clean working tree.
- Strict evidence gate output.
- Build/load path for the agent module.
- A2A Agent Card and task lifecycle evidence.
- Three use cases: personal file vault, agent services marketplace/payment hook, and multi-agent workflow.
- Spending threshold / owner approval / timeout behavior.
- Basecamp or owner-channel readiness, without claiming a GUI proof that is not shown.
- Honest limitations: paid-A2A LEZ tx binding and in-process LEZ program/CU proof are upstream/tooling-blocked unless fresh evidence proves otherwise.

## After recording

1. Upload the narrated video to a reviewer-accessible URL.
2. Run `scripts/finalize_after_video.sh "VIDEO_URL"`.
3. Re-run validators, including `python3 scripts/audit_lp0008_prize_criteria.py --final`.
4. Commit the URL and evidence updates.
5. Confirm CI green for the exact public SHA.
6. Ask explicit approval before opening the public Lambda Prize PR.
