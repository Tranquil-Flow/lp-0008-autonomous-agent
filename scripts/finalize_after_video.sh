#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <https-video-url>" >&2
  exit 2
fi

VIDEO_URL="$1"
IMPL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LAMBDA_ROOT="${LAMBDA_PRIZE_WORKTREE:-$HOME/lp0008-phase0/lambda-prize-local}"
UPSTREAM_URL="${LAMBDA_PRIZE_UPSTREAM_URL:-https://github.com/logos-co/lambda-prize.git}"
PR_TITLE="Solution: LP-0008 — Autonomous AI Agent Module with Wallet, Storage, and Messaging"

case "$VIDEO_URL" in
  https://*) ;;
  *) echo "video URL must start with https://" >&2; exit 2 ;;
esac

cd "$IMPL_ROOT"
python3 scripts/check_lp0008_submission_window.py
LAMBDA_PRIZE_WORKTREE="$LAMBDA_ROOT" scripts/apply_final_video_url.py "$VIDEO_URL"
python3 scripts/validate_acceptance_readiness.py
git diff --check

# Validate Lambda Prize solution with same trusted-base/untrusted-PR shape as CI.
TMP="$(mktemp -d /tmp/lp0008-finalize.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
git clone --quiet --depth 1 "$UPSTREAM_URL" "$TMP/base"
cp -R "$LAMBDA_ROOT" "$TMP/pr"
(
  cd "$TMP"
  CHANGED_FILES="solutions/LP-0008.md" \
  PR_TITLE="$PR_TITLE" \
  PR_REPO="Tranquil-Flow/lambda-prize" \
  BASE_REPO="logos-co/lambda-prize" \
  REPO_BLOB_URL="https://github.com/logos-co/lambda-prize/blob/master" \
    bash base/.github/scripts/validate-submission.sh
)

cat <<EOF

FINALIZE_OK

Next, if the diff looks correct:

  cd "$IMPL_ROOT"
  git add SUBMISSION.md
  git commit -m "docs: add final LP-0008 video URL"
  git push origin HEAD:main

  cd "$LAMBDA_ROOT"
  git add solutions/LP-0008.md
  git commit -m "solutions: LP-0008 final video URL"
  git push origin HEAD:lp0008-solution-final

Then ask explicit approval before opening a public Lambda Prize PR.
Suggested PR title:
  $PR_TITLE
EOF
