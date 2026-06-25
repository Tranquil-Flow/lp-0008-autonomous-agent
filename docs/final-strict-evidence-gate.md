# Final strict evidence gate

`scripts/run_final_strict_evidence.sh` is the non-video umbrella gate for LP-0008. It records repository SHA, runs the Nix build, raw C ABI demo, strict preflight, three-agent verification, paid-A2A evidence, Basecamp owner approval evidence, program/CU boundary evidence, strict default-skill matrix, readiness validator, and the legacy pre-video evidence bundle unless `LP0008_SKIP_PREVIDEO_BUNDLE=1` is set.

Outputs are retained under `~/lp0008-phase0/final_strict_evidence_<timestamp>/`, including `final_strict_evidence_summary.json` and per-step logs. The summary status is `ok`, `ok_with_blockers`, or `failed`. Known upstream/tooling blockers (for example rc3 wallet transfer panic or unavailable in-process program deploy/call API) are recorded explicitly rather than hidden.

Final-use command:

```bash
scripts/run_final_strict_evidence.sh
```

Development-only command while the worktree is dirty or when iterating quickly:

```bash
LP0008_ALLOW_DIRTY=1 LP0008_SKIP_PREVIDEO_BUNDLE=1 scripts/run_final_strict_evidence.sh
```

CI: `docs/live-strict-evidence.workflow.yml (copy to .github/workflows/live-strict-evidence.yml with workflow-scope token)` is manual-only (`workflow_dispatch`) because it needs macOS/Nix and live Logos/LEZ prerequisites.
