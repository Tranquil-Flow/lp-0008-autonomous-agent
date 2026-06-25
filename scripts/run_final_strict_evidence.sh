#!/usr/bin/env bash
# LP-0008 final strict evidence umbrella gate.
# Captures all current non-video strict evidence and writes a machine-readable summary.
# It does NOT open a Lambda Prize PR and does NOT claim video completion.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PATH="/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH"

TS="$(date -u +%Y%m%d_%H%M%S)"
EVIDENCE_ROOT="${LP0008_FINAL_STRICT_EVIDENCE_DIR:-$HOME/lp0008-phase0/final_strict_evidence_$TS}"
mkdir -p "$EVIDENCE_ROOT"
SUMMARY="$EVIDENCE_ROOT/final_strict_evidence_summary.json"
LOG_DIR="$EVIDENCE_ROOT/logs"
mkdir -p "$LOG_DIR"

say() { printf '\n\033[1;35m==> %s\033[0m\n' "$*"; }
json_quote() { python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'; }

REPO_SHA="$(git rev-parse HEAD)"
STATUS_SHORT="$(git status --short)"
if [ -n "$STATUS_SHORT" ] && [ "${LP0008_ALLOW_DIRTY:-0}" != "1" ]; then
  printf '%s\n' "$STATUS_SHORT" >&2
  echo "working tree must be clean for final strict evidence; set LP0008_ALLOW_DIRTY=1 only while developing the gate" >&2
  exit 2
fi

cat > "$SUMMARY" <<JSON
{
  "schema": "lp0008-final-strict-evidence-v1",
  "repo_sha": "$REPO_SHA",
  "started_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "evidence_root": "$EVIDENCE_ROOT",
  "allow_dirty": "${LP0008_ALLOW_DIRTY:-0}",
  "steps": []
}
JSON

append_step() {
  local name="$1" status="$2" exit_code="$3" log="$4" note="${5:-}"
  python3 - "$SUMMARY" "$name" "$status" "$exit_code" "$log" "$note" <<'PY'
import json, sys, datetime
path,name,status,exit_code,log,note=sys.argv[1:]
data=json.loads(open(path).read())
data.setdefault('steps',[]).append({
  'name': name,
  'status': status,
  'exit_code': int(exit_code),
  'log': log,
  'note': note,
  'recorded_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
})
open(path,'w').write(json.dumps(data, indent=2, sort_keys=True)+'\n')
PY
}

run_step_allow_blocker() {
  local name="$1" marker="$2"; shift 2
  local log="$LOG_DIR/$name.log"
  say "$name"
  printf '$ %s
' "$*" | tee "$log"
  set +e
  "$@" >> "$log" 2>&1
  local rc=$?
  set -e
  tail -40 "$log" || true
  if [ "$rc" -eq 0 ]; then
    append_step "$name" pass "$rc" "$log"
  elif grep -q "$marker" "$log"; then
    append_step "$name" blocked "$rc" "$log" "known fail-closed blocker marker: $marker"
  else
    append_step "$name" fail "$rc" "$log"
    echo "step failed: $name (rc=$rc), log=$log" >&2
    exit "$rc"
  fi
}

run_step() {
  local name="$1"; shift
  local log="$LOG_DIR/$name.log"
  say "$name"
  printf '$ %s\n' "$*" | tee "$log"
  set +e
  "$@" >> "$log" 2>&1
  local rc=$?
  set -e
  tail -40 "$log" || true
  if [ "$rc" -eq 0 ]; then
    append_step "$name" pass "$rc" "$log"
  else
    append_step "$name" fail "$rc" "$log"
    echo "step failed: $name (rc=$rc), log=$log" >&2
    exit "$rc"
  fi
}

say "Repository identity"
printf 'repo_sha=%s\n' "$REPO_SHA" | tee "$LOG_DIR/repo.log"
printf '%s\n' "$STATUS_SHORT" | tee -a "$LOG_DIR/repo.log"
append_step repo pass 0 "$LOG_DIR/repo.log" "repository identity captured"

# Core build/raw ABI proof first; verify_three_live_agents depends on demo-built cabi caller.
run_step build-install nix build .#install --out-link result
run_step raw-cabi-demo ./demo.sh

# Re-run the existing pre-video bundle before later strict scripts update evidence docs.
# The legacy gate intentionally requires a clean worktree.
if [ "${LP0008_SKIP_PREVIDEO_BUNDLE:-0}" = "1" ]; then
  append_step pre-video-bundle skipped 0 "$LOG_DIR/pre-video-bundle.log" "LP0008_SKIP_PREVIDEO_BUNDLE=1"
else
  run_step pre-video-bundle scripts/run_final_pre_video_evidence.sh
fi

# Strict live gate inventory and all phase-specific hardening evidence.
run_step strict-preflight scripts/preflight_strict_live_gates.sh

THREE_STATE="${LP0008_THREE_AGENT_STATE:-$HOME/lp0008-phase0/three_live_agents_strict_latest}"
WALLET_CLI="${LP0008_WALLET_CLI:-$HOME/Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet}"
run_step three-live-agents scripts/verify_three_live_agents.py --state-root "$THREE_STATE" --wallet-cli "$WALLET_CLI"
run_step_allow_blocker paid-a2a-live PAID_A2A_BLOCKED scripts/run_paid_a2a_live_evidence.py
run_step basecamp-owner-approval scripts/run_basecamp_owner_approval_evidence.py
run_step program-cu-boundary scripts/run_program_cu_boundary_evidence.py
run_step strict-skill-matrix scripts/run_strict_skill_matrix.py
run_step acceptance-readiness scripts/validate_acceptance_readiness.py

python3 - "$SUMMARY" <<'PY'
import json, pathlib, sys, datetime
path=pathlib.Path(sys.argv[1])
data=json.loads(path.read_text())
steps=data.get('steps', [])
failed=[s for s in steps if s['status']=='fail']
skipped=[s for s in steps if s['status']=='skipped']
blocked_steps=[s for s in steps if s['status']=='blocked']
# Inspect known blocker JSONs from paid/program evidence. They are honest evidence, not shell failures.
root=pathlib.Path(data['evidence_root'])
phase_root=pathlib.Path.home()/'lp0008-phase0'
blockers=[]
for p in phase_root.glob('paid_a2a_live_*/paid_a2a_live_blocker.json'):
    try:
        j=json.loads(p.read_text())
        if j.get('blocked'): blockers.append({'kind':'paid_a2a', 'path':str(p), 'reason':j.get('blocker') or j.get('error')})
    except Exception: pass
for p in phase_root.glob('program_cu_boundary_*/program_cu_boundary_summary.json'):
    try:
        j=json.loads(p.read_text())
        if j.get('program_call',{}).get('error') or j.get('program_deploy',{}).get('error'):
            blockers.append({'kind':'program_boundary', 'path':str(p), 'reason':'program call/deploy fail closed until stable in-process API exists'})
    except Exception: pass
data['finished_at']=datetime.datetime.now(datetime.timezone.utc).isoformat()
data['failed_steps']=failed
data['skipped_steps']=skipped
data['blocked_steps']=blocked_steps
data['blockers']=blockers[-5:]
data['status']='failed' if failed else ('ok_with_blockers' if blockers or blocked_steps else 'ok')
path.write_text(json.dumps(data, indent=2, sort_keys=True)+'\n')
print(json.dumps({'status':data['status'], 'steps':len(steps), 'failed':len(failed), 'skipped':len(skipped), 'blocked_steps':len(blocked_steps), 'blockers':len(blockers), 'summary':str(path)}, indent=2, sort_keys=True))
if failed:
    raise SystemExit(1)
if blockers and bool(int(__import__('os').environ.get('LP0008_FAIL_ON_BLOCKERS','0'))):
    raise SystemExit(3)
PY

say "FINAL_STRICT_EVIDENCE_COMPLETE"
echo "$SUMMARY"
