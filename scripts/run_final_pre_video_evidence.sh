#!/usr/bin/env bash
# LP-0008 strict-readiness evidence gate.
#
# Runs the current non-video proof bundle before any final narrated recording:
#   - Nix package build
#   - raw C ABI demo for all 23 skills
#   - Logos Core co-load with storage_module and delivery_module
#   - live deep verifier for storage, delivery, A2A transport, and task lifecycle
#   - three-agent A2A lifecycle proof
#   - public-claim readiness validator
#   - optional live LEZ wallet send when funded wallet env vars are present
#
# This script deliberately does not require the final video URL. Passing it means
# current evidence is internally consistent; it does not by itself close every
# strict LP-0008 acceptance risk documented in docs/submission-readiness-matrix.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export PATH="/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH"

say() { printf '\n\033[1;35m==> %s\033[0m\n' "$*"; }

say "Repository identity"
git rev-parse HEAD
git status --short

test -z "$(git status --short)" || { echo "working tree must be clean before final evidence" >&2; exit 1; }

say "Build installable Logos module"
nix build .#install --out-link result

test -d result/modules/agent_module

say "Raw C ABI demo: all 23 skills + spending gate"
./demo.sh

say "Logos Core integration: storage_module + delivery_module live paths"
scripts/run_logoscore_integration.sh all

say "Deep verifier: live storage, live delivery, live A2A task transport"
scripts/run_lp0008_deep_verify.py

say "Three-agent A2A control-plane proof"
scripts/run_multi_agent_a2a_demo.sh

say "Three illustrative use cases proof"
scripts/run_three_use_cases_demo.sh

say "Persistence/fail-closed/failure-isolation resilience proof"
scripts/run_resilience_evidence.sh

say "Reviewer-facing readiness validator"
scripts/validate_acceptance_readiness.py

if [ -n "${LP0008_LIVE_WALLET_HOME:-}" ] && [ -n "${LP0008_LIVE_WALLET_ACCOUNT:-}" ] && [ -n "${LP0008_LIVE_SEND_RECIPIENT:-}" ]; then
  say "Live public-testnet LEZ wallet send proof"
  scripts/run_live_wallet_send_verify.py
else
  say "Live wallet send proof skipped in this environment"
  echo "Set LP0008_LIVE_WALLET_HOME, LP0008_LIVE_WALLET_ACCOUNT, and LP0008_LIVE_SEND_RECIPIENT to run it."
fi

say "PRE_VIDEO_EVIDENCE_OK"
echo "Current evidence bundle is green. Strict residual risks remain documented in docs/submission-readiness-matrix.md; do not claim final submission readiness until those are closed or explicitly accepted."
