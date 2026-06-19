#!/usr/bin/env bash
# Reusable LP-0008 Logos Core integration smoke test.
#
# Exercises the real logoscore runtime with progressively co-loaded modules:
#   Stage B: agent_module + storage_module
#   Stage C: agent_module + storage_module + delivery_module
#   Safety: invalid delivery topic remains local and does not crash delivery_module
#
# Defaults match the M4 Pro Logos Basecamp workspace. Override with env vars.
set -euo pipefail

ROOT=${LOGOS_BASECAMP_ROOT:-$HOME/Projects/logos-basecamp}

resolve_logoscore_cli() {
  if [ -n "${LOGOSCORE_CLI:-}" ]; then
    echo "$LOGOSCORE_CLI"
    return
  fi
  if command -v logoscore >/dev/null 2>&1; then
    command -v logoscore
    return
  fi
  local out="$OUT_ROOT/logoscore-cli"
  if [ ! -x "$out/bin/logoscore" ]; then
    nix build github:logos-co/logos-logoscore-cli#default --out-link "$out" >/dev/null
  fi
  echo "$out/bin/logoscore"
}

AGENT_REPO=${AGENT_REPO:-$ROOT/lp-0008-autonomous-agent}
STORAGE_REPO=${STORAGE_REPO:-$ROOT/refs/logos-storage-module}
DELIVERY_REPO=${DELIVERY_REPO:-$ROOT/refs/logos-delivery-module}
OUT_ROOT=${LP0008_TEST_ROOT:-$HOME/lp0008-phase0}
CLI=$(resolve_logoscore_cli)

AGENT_MODULES=${AGENT_MODULES:-$AGENT_REPO/result/modules}
STORAGE_MODULES=${STORAGE_MODULES:-$STORAGE_REPO/result/modules}
DELIVERY_MODULES=${DELIVERY_MODULES:-$DELIVERY_REPO/result/modules}

json_args() {
  python3 - "$@" <<'PY'
import json, sys
print(json.dumps(sys.argv[1:]))
PY
}

require_file() {
  if [ ! -e "$1" ]; then
    echo "missing required path: $1" >&2
    exit 1
  fi
}

build_if_needed() {
  local repo=$1
  local expected=$2
  local label=$3
  if [ -e "$expected" ]; then
    return 0
  fi
  echo "building $label in $repo"
  (cd "$repo" && nix build .#install)
}

start_daemon() {
  local base=$1
  shift
  mkdir -p "$base/config" "$base/persist"
  "$CLI" --config-dir "$base/config" "$@" --persistence-path "$base/persist" --persist-config daemon > "$base/daemon.log" 2>&1 &
  DAEMON_PID=$!
  echo "$DAEMON_PID" > "$base/daemon.pid"
  for _ in {1..80}; do
    if "$CLI" --config-dir "$base/config" list-modules >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  echo "logoscore daemon did not become ready; tail follows" >&2
  tail -120 "$base/daemon.log" >&2 || true
  return 1
}

stop_daemon() {
  if [ -n "${DAEMON_PID:-}" ]; then
    kill "$DAEMON_PID" 2>/dev/null || true
    wait "$DAEMON_PID" 2>/dev/null || true
  fi
}

parse_result_field() {
  python3 - "$1" <<'PY'
import json, pathlib, sys
outer=json.loads(pathlib.Path(sys.argv[1]).read_text())
if outer.get('status') != 'ok':
    raise SystemExit(f"call failed: {outer}")
print(json.dumps(json.loads(outer['result'])))
PY
}

run_stage_b() {
  local base=$OUT_ROOT/integration_stage_b_$(date +%s)
  mkdir -p "$base"
  local input=$base/input.txt
  echo "stage-b storage $(date)" > "$input"
  DAEMON_PID=""
  trap stop_daemon RETURN
  start_daemon "$base" -m "$STORAGE_MODULES" -m "$AGENT_MODULES"
  "$CLI" --config-dir "$base/config" load-module storage_module | tee "$base/load-storage.out"
  "$CLI" --config-dir "$base/config" load-module agent_module | tee "$base/load-agent.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill storage.upload "$(json_args "$input" stage-b-file)" | tee "$base/storage-upload.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill storage.list '[]' | tee "$base/storage-list.out"
  python3 - "$base" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1])
upload=json.loads(json.loads((base/'storage-upload.out').read_text())['result'])
listing=json.loads(json.loads((base/'storage-list.out').read_text())['result'])
assert upload['stored'] is True and upload['mode'] == 'live', upload
assert listing['count'] >= 1 and any(f.get('label') == 'stage-b-file' for f in listing['files']), listing
assert listing['mode'] == 'live', listing
print('ASSERT Stage B storage live upload/list: OK')
PY
  echo "STAGE_B_BASE=$base"
}

run_stage_c() {
  local base=$OUT_ROOT/integration_stage_c_$(date +%s)
  mkdir -p "$base"
  local input=$base/input.txt
  echo "stage-c storage+delivery $(date)" > "$input"
  DAEMON_PID=""
  trap stop_daemon RETURN
  start_daemon "$base" -m "$STORAGE_MODULES" -m "$DELIVERY_MODULES" -m "$AGENT_MODULES"
  "$CLI" --config-dir "$base/config" load-module storage_module | tee "$base/load-storage.out"
  "$CLI" --config-dir "$base/config" load-module delivery_module | tee "$base/load-delivery.out"
  "$CLI" --config-dir "$base/config" load-module agent_module | tee "$base/load-agent.out"
  "$CLI" --config-dir "$base/config" list-modules | tee "$base/list-modules.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill storage.upload "$(json_args "$input" stage-c-file)" | tee "$base/storage-upload.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill storage.list '[]' | tee "$base/storage-list.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill messaging.send "$(json_args /lp0008/1/stage-c/proto 'hello from lp0008 stage c')" | tee "$base/message-send.out"
  python3 - "$base" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1])
mods=json.loads((base/'list-modules.out').read_text())
loaded={m['name'] for m in mods if m.get('status') == 'loaded'}
assert {'storage_module','delivery_module','agent_module'}.issubset(loaded), loaded
listing=json.loads(json.loads((base/'storage-list.out').read_text())['result'])
msg=json.loads(json.loads((base/'message-send.out').read_text())['result'])
assert listing['count'] >= 1 and any(f.get('label') == 'stage-c-file' for f in listing['files']), listing
assert msg['sent'] is True and msg['mode'] == 'live' and msg.get('message_id'), msg
print('ASSERT Stage C storage+delivery live path: OK')
PY
  echo "STAGE_C_BASE=$base"
}

run_invalid_topic_guard() {
  local base=$OUT_ROOT/integration_invalid_topic_guard_$(date +%s)
  mkdir -p "$base"
  DAEMON_PID=""
  trap stop_daemon RETURN
  start_daemon "$base" -m "$DELIVERY_MODULES" -m "$AGENT_MODULES"
  "$CLI" --config-dir "$base/config" load-module delivery_module | tee "$base/load-delivery.out"
  "$CLI" --config-dir "$base/config" load-module agent_module | tee "$base/load-agent.out"
  "$CLI" --config-dir "$base/config" call agent_module dispatchSkill messaging.send '["/logos/lp0008/stage-c", "invalid topic guard"]' | tee "$base/invalid-topic.out"
  "$CLI" --config-dir "$base/config" list-modules | tee "$base/list-after.out"
  python3 - "$base" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1])
msg=json.loads(json.loads((base/'invalid-topic.out').read_text())['result'])
mods=json.loads((base/'list-after.out').read_text())
loaded={m['name'] for m in mods if m.get('status') == 'loaded'}
assert msg['sent'] is True and msg['mode'] == 'simulated', msg
assert 'not a valid LIP-23' in msg.get('note', ''), msg
assert {'delivery_module','agent_module'}.issubset(loaded), loaded
print('ASSERT invalid topic guarded locally; delivery_module remains loaded: OK')
PY
  echo "INVALID_TOPIC_BASE=$base"
}

main() {
  require_file "$CLI"
  mkdir -p "$OUT_ROOT"
  build_if_needed "$AGENT_REPO" "$AGENT_MODULES/agent_module" agent_module
  build_if_needed "$STORAGE_REPO" "$STORAGE_MODULES/storage_module" storage_module
  build_if_needed "$DELIVERY_REPO" "$DELIVERY_MODULES/delivery_module" delivery_module
  require_file "$AGENT_MODULES/agent_module/manifest.json"
  require_file "$STORAGE_MODULES/storage_module/manifest.json"
  require_file "$DELIVERY_MODULES/delivery_module/manifest.json"

  case "${1:-all}" in
    stage-b) run_stage_b ;;
    stage-c) run_stage_c ;;
    invalid-topic) run_invalid_topic_guard ;;
    all)
      run_stage_b
      run_stage_c
      run_invalid_topic_guard
      ;;
    *)
      echo "usage: $0 [all|stage-b|stage-c|invalid-topic]" >&2
      exit 2
      ;;
  esac
}

main "$@"
