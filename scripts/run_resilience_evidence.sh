#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

STATE_ROOT=${LP0008_RESILIENCE_STATE:-$HOME/lp0008-phase0/resilience_$(date +%s)}
CALLER="$STATE_ROOT/cabi_call"
PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.dylib"
if [ ! -f "$PLUGIN" ]; then
  PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.so"
fi
mkdir -p "$STATE_ROOT"
export AGENT_MODULE_STATE_DIR="$STATE_ROOT"
export LP0008_DISABLE_WALLET_FFI=${LP0008_DISABLE_WALLET_FFI:-1}

nix build .#install --out-link result >/dev/null
nix develop --command c++ -std=c++17 -O2 -o "$CALLER" tests/cabi_call.cpp >/dev/null

json_call_args() {
  python3 - "$1" "$2" <<'PY'
import json, sys
print(json.dumps([sys.argv[1], sys.argv[2]]))
PY
}
call_skill() {
  local skill=$1 args=$2 out=$3
  "$CALLER" "$PLUGIN" dispatchSkill "$(json_call_args "$skill" "$args")" | tee "$out"
}
json_array() {
  python3 - "$@" <<'PY'
import json, sys
print(json.dumps(sys.argv[1:]))
PY
}

# Configure a tiny per-tx limit so a 10 LEZ send must go through owner approval.
call_skill meta.configure '["agent_id","resilience-agent"]' "$STATE_ROOT/config-agent.out" >/dev/null
call_skill meta.configure '["owner_topic","/lp0008/1/resilience/owner"]' "$STATE_ROOT/config-owner.out" >/dev/null
call_skill meta.configure '["per_tx_limit","1"]' "$STATE_ROOT/config-limit.out" >/dev/null
call_skill wallet.send "$(json_array yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3 0a000000000000000000000000000000)" "$STATE_ROOT/approval-request.out" >/dev/null
APPROVAL_ID=$(python3 - "$STATE_ROOT/approval-request.out" <<'PY'
import json, pathlib, sys
print(json.loads(json.loads(pathlib.Path(sys.argv[1]).read_text()))['approval_id'])
PY
)

# Simulated restart: every call is a new process; this explicit second caller invocation must reload approvals.json.
call_skill approval.list '[]' "$STATE_ROOT/approval-list-after-restart.out" >/dev/null

# Force timeout by editing persisted state, then verify approve/retry fail closed and status persists.
python3 - "$STATE_ROOT" "$APPROVAL_ID" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1]); aid=sys.argv[2]
p=base/'approvals.json'
arr=json.loads(p.read_text())
for approval in arr:
    if approval.get('approval_id') == aid:
        approval['expires_at'] = 1
p.write_text(json.dumps(arr, indent=2, sort_keys=True))
PY
call_skill approval.retry "$(json_array "$APPROVAL_ID")" "$STATE_ROOT/approval-retry-expired.out" >/dev/null
call_skill approval.approve "$(json_array "$APPROVAL_ID")" "$STATE_ROOT/approval-approve-after-timeout.out" >/dev/null
call_skill approval.list '[]' "$STATE_ROOT/approval-list-after-timeout.out" >/dev/null

# Skill failure isolation: failed task must not poison later successful skill dispatch.
call_skill agent.task "$(json_array resilience-agent agent.no_such_skill '{}')" "$STATE_ROOT/task-failure.out" >/dev/null
VAULT_IN="$STATE_ROOT/post-failure-file.txt"
VAULT_OUT="$STATE_ROOT/post-failure-file-restored.txt"
printf 'post-failure isolation %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$VAULT_IN"
call_skill storage.upload "$(json_array "$VAULT_IN" post-failure-isolation)" "$STATE_ROOT/post-failure-upload.out" >/dev/null
ADDR=$(python3 - "$STATE_ROOT/post-failure-upload.out" <<'PY'
import json, pathlib, sys
print(json.loads(json.loads(pathlib.Path(sys.argv[1]).read_text()))['address'])
PY
)
call_skill storage.download "$(json_array "$ADDR" "$VAULT_OUT")" "$STATE_ROOT/post-failure-download.out" >/dev/null
cmp "$VAULT_IN" "$VAULT_OUT"

python3 - "$STATE_ROOT" "$APPROVAL_ID" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1]); aid=sys.argv[2]
def parse(name):
    return json.loads(json.loads((base/name).read_text()))
request=parse('approval-request.out')
listed=parse('approval-list-after-restart.out')
retry=parse('approval-retry-expired.out')
approve=parse('approval-approve-after-timeout.out')
listed_timeout=parse('approval-list-after-timeout.out')
failure=parse('task-failure.out')
upload=parse('post-failure-upload.out')
download=parse('post-failure-download.out')
assert request.get('action') == 'owner_approval_required' and request.get('approval_id') == aid, request
matches=[a for a in listed.get('approvals', []) if a.get('approval_id') == aid]
assert matches and matches[0].get('status') == 'pending', listed
assert retry.get('notified') is False and retry.get('error') == 'approval_expired' and retry.get('status') == 'timed_out', retry
assert approve.get('approved') is False and approve.get('error') == 'approval_not_pending' and approve.get('status') == 'timed_out', approve
matches=[a for a in listed_timeout.get('approvals', []) if a.get('approval_id') == aid]
assert matches and matches[0].get('status') == 'timed_out', listed_timeout
assert failure.get('status') == 'failed' and failure.get('error', {}).get('error') == 'unknown_skill', failure
assert upload.get('stored') is True and upload.get('address'), upload
assert download.get('downloaded') is True and pathlib.Path(download['path']).exists(), download
summary={
  'approval_id': aid,
  'pending_survived_restart': True,
  'expired_retry_failed_closed': True,
  'expired_approval_not_executed': True,
  'post_failure_skill_isolated': True,
  'evidence_dir': str(base),
}
(base/'summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True))
print(json.dumps(summary, indent=2, sort_keys=True))
print('ASSERT resilience: pending approval persisted across restart; expired approval failed closed; failed task isolated from later storage skill: OK')
PY
