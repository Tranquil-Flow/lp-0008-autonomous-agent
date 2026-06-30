# LP-0008 final submission preflight

Purpose: reviewer-facing checklist for the last mile before opening a public Lambda Prize PR. It is intentionally conservative: non-video repository hardening complete or explicitly upstream/tooling-blocked is necessary, but not enough, because the final narrated video URL and explicit approval are still required.

## Required order

1. Start from a clean checkout of the public repository and confirm `git status --short` is empty.
2. Run the strict umbrella gate:

```bash
scripts/run_final_strict_evidence.sh
```

Expected status before upstream wallet/program APIs improve: `FINAL_STRICT_EVIDENCE_COMPLETE` with `ok_with_blockers`, not silent success. Blockers must be limited to documented upstream/tooling limits.

3. Run reviewer-facing validators:

```bash
python3 tests/validate_traceability_freshness.py
python3 scripts/validate_acceptance_readiness.py
python3 scripts/audit_lp0008_prize_criteria.py
python3 scripts/check_lp0008_submission_window.py
```

4. Record the narrated demo using `scripts/record_final_video.sh` and the narration in `docs/final-video-audio-narration.md`.
5. Replace `PENDING_VIDEO_URL` with the uploaded reviewer-accessible video URL:

```bash
scripts/finalize_after_video.sh "https://example.invalid/final-video"
```

6. Re-run validators after URL insertion. For final publication, `python3 scripts/audit_lp0008_prize_criteria.py --final` must pass.
7. Confirm CI is green for the exact public SHA:

```bash
python3 scripts/check_github_ci.py
```

8. Ask explicit approval before opening the public Lambda Prize PR.

## Claim boundaries

- The strict gate is the authoritative non-video evidence bundle.
- The older `scripts/run_final_pre_video_evidence.sh` remains available and may be nested inside the strict gate; it is not the final readiness claim by itself.
- The repository must continue to say `PENDING_VIDEO_URL` until the real uploaded narrated video exists.
- Paid A2A LEZ tx binding and in-process program deploy/call/CU proof remain upstream/tooling-blocked unless fresh evidence proves otherwise.
