# LP-0008 CI evidence

`scripts/check_github_ci.py` verifies the implementation repository default branch is green for the exact public SHA under review.

The checker is intentionally read-only:

1. Fetches `origin/main`.
2. Requires `origin/main == git rev-parse HEAD` so feature-only or stale local commits cannot be mistaken for public evidence.
3. Queries GitHub Actions for `Tranquil-Flow/lp-0008-autonomous-agent` on `main` using `gh` when available, with unauthenticated REST fallback for public repo checks.
4. Requires a run whose `headSha` equals the exact SHA and whose status is `completed` with conclusion `success`.

Reviewer-facing boundary: this proves default CI/build/demo hygiene only. It does not prove final video, paid-A2A LEZ settlement, Basecamp GUI owner-channel interaction, or in-process program/CU proof paths.

Expected success marker:

```text
LP0008_CI_OK sha=<public HEAD> run=<Actions run id> url=<Actions run URL>
```
