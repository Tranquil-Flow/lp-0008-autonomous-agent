#!/usr/bin/env bash
# Fresh-clone reproduction proof for evaluator-style LP-0008 checks.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH"
TS="$(date -u +%Y%m%d_%H%M%S)"
EVIDENCE_ROOT="${LP0008_FRESH_CLONE_EVIDENCE_DIR:-$HOME/lp0008-phase0/fresh_clone_repro_$TS}"
mkdir -p "$EVIDENCE_ROOT"
SUMMARY="$EVIDENCE_ROOT/fresh_clone_repro_summary.json"
REPO_URL="${LP0008_FRESH_CLONE_URL:-file://$ROOT}"
CLONE_DIR="$EVIDENCE_ROOT/clone"
LOG="$EVIDENCE_ROOT/fresh_clone_repro.log"
: > "$LOG"
run() {
  printf '\n=== %s ===\n' "$*" | tee -a "$LOG"
  "$@" 2>&1 | tee -a "$LOG"
}
run git clone --depth 1 "$REPO_URL" "$CLONE_DIR"
cd "$CLONE_DIR"
run git rev-parse HEAD
run nix build .#install --out-link result
RUN_COMMANDS=0 SECTION_PAUSE=0 COMMAND_PAUSE=0 SCENE_PAUSE=0 run bash scripts/record_final_video.sh
run scripts/validate_acceptance_readiness.py
python3 - "$SUMMARY" "$ROOT" "$CLONE_DIR" "$LOG" "$REPO_URL" <<'PY'
import json, pathlib, subprocess, sys, datetime
summary, root, clone, log, repo_url = sys.argv[1:]
sha = subprocess.check_output(['git','rev-parse','HEAD'], cwd=clone, text=True).strip()
data = {
  'ok': True,
  'schema': 'lp0008-fresh-clone-strict-repro-v1',
  'repo_url': repo_url,
  'repo_sha': sha,
  'command_line': 'scripts/run_fresh_clone_strict_repro.sh',
  'checks': ['git clone', 'nix build .#install', 'RUN_COMMANDS=0 bash scripts/record_final_video.sh', 'scripts/validate_acceptance_readiness.py'],
  'clone_dir': clone,
  'log': log,
  'recorded_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
  'limitations': ['Default URL may be local file:// for pre-push evidence; set LP0008_FRESH_CLONE_URL=https://github.com/Tranquil-Flow/lp-0008-autonomous-agent.git for public clone proof.']
}
pathlib.Path(summary).write_text(json.dumps(data, indent=2, sort_keys=True)+'\n')
PY
printf 'FRESH_CLONE_STRICT_REPRO_OK %s\n' "$SUMMARY"
