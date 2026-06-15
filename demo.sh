#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

export AGENT_MODULE_STATE_DIR="${AGENT_MODULE_STATE_DIR:-$ROOT/.demo-state}"
CALLER="$ROOT/.demo-state/cabi_call"
PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.dylib"
ASSERT="$ROOT/tests/assert_result.py"

say() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
call() {
  local method="$1"
  local args_json="$2"
  local out="$3"
  "$CALLER" "$PLUGIN" "$method" "$args_json" | tee "$out"
}
assert_field() {
  python3 "$ASSERT" "$1" "$2" "$3"
}

say "Clean demo state"
rm -rf "$AGENT_MODULE_STATE_DIR"
mkdir -p "$AGENT_MODULE_STATE_DIR"

say "Build installable Logos module"
nix build .#install --out-link result

test -f "$PLUGIN"

say "Compile direct C ABI caller used for truthful return-value checks"
nix develop --command c++ -std=c++17 -O2 -o "$CALLER" tests/cabi_call.cpp

say "1. Module greets and reports loaded skills"
call greet '["Evi"]' "$AGENT_MODULE_STATE_DIR/greet.out"
python3 - <<'PY' "$AGENT_MODULE_STATE_DIR/greet.out"
import json, sys
raw=open(sys.argv[1]).read().strip()
value=json.loads(raw)
assert "Agent module v0.1.0" in value, value
assert "skills" in value, value
print("ok greet returned module identity string")
PY

say "2. Storage persists across separate module processes"
call storeData '["mission","protect liberty in the digital night"]' "$AGENT_MODULE_STATE_DIR/store.out"
assert_field "$AGENT_MODULE_STATE_DIR/store.out" .stored true
assert_field "$AGENT_MODULE_STATE_DIR/store.out" .backend '"file"'

# This is a separate process. It only succeeds if file-backed persistence works.
call retrieveData '["mission"]' "$AGENT_MODULE_STATE_DIR/retrieve.out"
assert_field "$AGENT_MODULE_STATE_DIR/retrieve.out" .found true
assert_field "$AGENT_MODULE_STATE_DIR/retrieve.out" .data '"protect liberty in the digital night"'

call listStored '[]' "$AGENT_MODULE_STATE_DIR/list.out"
assert_field "$AGENT_MODULE_STATE_DIR/list.out" .count 1
assert_field "$AGENT_MODULE_STATE_DIR/list.out" .keys.0 '"mission"'

say "3. Spending gate allows small transfer, records spend, blocks oversized transfer"
# LE16 amount 10 decimal: byte 0x0a followed by zeros.
SMALL='0a000000000000000000000000000000'
# LE16 amount 390625000 decimal, above default per-tx limit 1000.
HUGE='e87648170000000000000000000000'

call checkSpend "[\"$SMALL\"]" "$AGENT_MODULE_STATE_DIR/check-small.out"
assert_field "$AGENT_MODULE_STATE_DIR/check-small.out" .allowed true
assert_field "$AGENT_MODULE_STATE_DIR/check-small.out" .amount '"10"'

call transfer "[\"agent-wallet\",\"recipient-wallet\",\"$SMALL\"]" "$AGENT_MODULE_STATE_DIR/transfer-small.out"
assert_field "$AGENT_MODULE_STATE_DIR/transfer-small.out" .success true
assert_field "$AGENT_MODULE_STATE_DIR/transfer-small.out" .recorded true

# Separate process. It only sees history if spend persistence works.
call getSpendingHistory '[]' "$AGENT_MODULE_STATE_DIR/history.out"
assert_field "$AGENT_MODULE_STATE_DIR/history.out" .period_total '"10"'
assert_field "$AGENT_MODULE_STATE_DIR/history.out" .records.0.amount '"10"'

call checkSpend "[\"$HUGE\"]" "$AGENT_MODULE_STATE_DIR/check-huge.out"
assert_field "$AGENT_MODULE_STATE_DIR/check-huge.out" .allowed false
assert_field "$AGENT_MODULE_STATE_DIR/check-huge.out" .reason '"exceeds_per_tx_limit"'

say "4. Agent Card exposes A2A capability metadata"
call getAgentCard '[]' "$AGENT_MODULE_STATE_DIR/agent-card.out"
assert_field "$AGENT_MODULE_STATE_DIR/agent-card.out" .protocol '"a2a"'
assert_field "$AGENT_MODULE_STATE_DIR/agent-card.out" .status '"active"'

say "Demo passed: module builds, returns raw JSON, persists storage/history, and enforces spending gate."
