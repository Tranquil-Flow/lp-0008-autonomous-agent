#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

export AGENT_MODULE_STATE_DIR="${AGENT_MODULE_STATE_DIR:-$ROOT/.demo-state}"
CALLER="$ROOT/.demo-state/cabi_call"
PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.dylib"
ASSERT="$ROOT/tests/assert_result.py"
PY="python3"

say() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

# Call dispatchSkill(method, args) via the C ABI directly.
# Uses Python to safely construct the JSON array with proper escaping.
call() {
  local skill="$1"
  local args="$2"
  local out="$AGENT_MODULE_STATE_DIR/$(echo "$skill" | tr '.' '_').out"
  local json
  json=$("$PY" -c "import json,sys; print(json.dumps([sys.argv[1], sys.argv[2]]))" "$skill" "$args")
  "$CALLER" "$PLUGIN" dispatchSkill "$json" > "$out"
  cat "$out"
  echo
}

# Assert a field in a previous call's output
assert() {
  local skill="$1"
  local path="$2"
  local expected="$3"
  local out="$AGENT_MODULE_STATE_DIR/$(echo "$skill" | tr '.' '_').out"
  "$PY" "$ASSERT" "$out" "$path" "$expected"
}

# Get parsed field value from previous call output
getval() {
  local skill="$1"
  local path="$2"
  local out="$AGENT_MODULE_STATE_DIR/$(echo "$skill" | tr '.' '_').out"
  "$PY" -c "
import sys; sys.path.insert(0, '$ROOT/tests')
from assert_result import smart_parse
v = smart_parse(open('$out').read().strip())
for p in '$path'.lstrip('.').split('.'):
    if p: v = v[int(p)] if isinstance(v, list) else v[p]
print(v)
"
}

# ═══════════════════════════════════════════════════════════════════════
say "Clean demo state"
rm -rf "$AGENT_MODULE_STATE_DIR"
mkdir -p "$AGENT_MODULE_STATE_DIR"

say "Build installable Logos module"
nix build .#install --out-link result
test -f "$PLUGIN"

say "Compile direct C ABI caller for truthful return verification"
nix develop --command c++ -std=c++17 -O2 -o "$CALLER" tests/cabi_call.cpp

# ═══════════════════════════════════════════════════════════════════════
say "META: Skills listing, status, configuration"
# ═══════════════════════════════════════════════════════════════════════

call greet '["Evi"]'
assert greet . '"Agent module v0.1.0"'

call meta.skills '[]'
$PY -c "
import sys; sys.path.insert(0,'$ROOT/tests')
from assert_result import smart_parse
v=smart_parse(open('$AGENT_MODULE_STATE_DIR/meta_skills.out').read().strip())
assert isinstance(v,list) and len(v)>=20, f'expected >=20 skills, got {len(v)}'
print(f'ok meta.skills lists {len(v)} skills')
"

call meta.status '[]'
assert meta.status .status '"running"'

call meta.configure '["per_tx_limit","500"]'
assert meta.configure .updated true

# ═══════════════════════════════════════════════════════════════════════
say "STORAGE: Upload, download, list, share"
# ═══════════════════════════════════════════════════════════════════════

echo "protect liberty in the digital night" > "$AGENT_MODULE_STATE_DIR/secret.txt"

call storage.upload "[\"$AGENT_MODULE_STATE_DIR/secret.txt\",\"mission-doc\"]"
assert storage.upload .stored true
assert storage.upload .backend '"file"'

ADDR=$(getval storage.upload .address)
echo "  Uploaded address: $ADDR"

call storage.download "[\"$ADDR\",\"$AGENT_MODULE_STATE_DIR/retrieved.txt\"]"
assert storage.download .downloaded true

call storage.list '[]'
assert storage.list .count 1

call storage.share "[\"$ADDR\",\"0x-friend\"]"
assert storage.share .shared true

# ═══════════════════════════════════════════════════════════════════════
say "MESSAGING: Send, join, create_group"
# ═══════════════════════════════════════════════════════════════════════

call messaging.send '["0x-owner","Agent online. Standing by."]'
assert messaging.send .sent true

call messaging.join '["\/logos\/groups\/dao-voting"]'
assert messaging.join .joined true

call messaging.create_group '["[\"0x-alpha\",\"0x-beta\"]"]'
assert messaging.create_group .created true

# ═══════════════════════════════════════════════════════════════════════
say "WALLET: Balance, send (with spending gate), history"
# ═══════════════════════════════════════════════════════════════════════

call wallet.balance '[]'
assert wallet.balance .balance '"0"'

# Small transfer — within threshold (per_tx_limit=500)
# LE16 for 10 decimal
SMALL='0a000000000000000000000000000000'
call wallet.send "[\"0x-recipient\",\"$SMALL\"]"
assert wallet.send .approved true

# Large transfer — above threshold
# LE16 for 390625000
HUGE='e87648170000000000000000000000'
call wallet.send "[\"0x-recipient\",\"$HUGE\"]"
assert wallet.send .approved false

call wallet.history '[]'
$PY -c "
import sys; sys.path.insert(0,'$ROOT/tests')
from assert_result import smart_parse
v=smart_parse(open('$AGENT_MODULE_STATE_DIR/wallet_history.out').read().strip())
txs=v.get('transactions',[])
assert len(txs)>=1, f'expected >=1 tx, got {len(txs)}'
print(f'ok wallet.history has {len(txs)} transaction(s)')
"

# ═══════════════════════════════════════════════════════════════════════
say "PROGRAM: Query, call, deploy"
# ═══════════════════════════════════════════════════════════════════════

call program.query '["0x-gov","{\"proposal_id\":42}"]'
assert program.query .program_id '"0x-gov"'

call program.call '["0x-gov","vote","{\"proposal_id\":42,\"support\":true}"]'
assert program.call .submitted true

echo "dummy-program-binary" > "$AGENT_MODULE_STATE_DIR/prog.bin"
call program.deploy "[\"$AGENT_MODULE_STATE_DIR/prog.bin\"]"
assert program.deploy .deployed true

# ═══════════════════════════════════════════════════════════════════════
say "AGENT (A2A): Card, discover, task, subscribe, cancel"
# ═══════════════════════════════════════════════════════════════════════

call agent.card '[]'
assert agent.card .protocol '"a2a"'
assert agent.card .status '"active"'
$PY -c "
import sys; sys.path.insert(0,'$ROOT/tests')
from assert_result import smart_parse
v=smart_parse(open('$AGENT_MODULE_STATE_DIR/agent_card.out').read().strip())
assert 'payment' in v, 'card must declare payment'
skills=v.get('skills',[])
assert len(skills)>=20, f'card has {len(skills)} skills'
print(f'ok agent.card: A2A-compatible, {len(skills)} skills, payment declared')
"

call agent.discover '["\/logos\/agents\/v1\/discovery"]'
assert agent.discover .topic '"/logos/agents/v1/discovery"'

call agent.task '["0x-beta","storage.upload","{\"label\":\"handoff\"}"]'
assert agent.task .status '"working"'

TASK_ID=$(getval agent.task .task_id)

call agent.subscribe "[\"0x-beta\",\"$TASK_ID\"]"
assert agent.subscribe .subscribed true

call agent.cancel "[\"0x-beta\",\"$TASK_ID\"]"
assert agent.cancel .cancelled true

# ═══════════════════════════════════════════════════════════════════════
say "ALL 21 SKILLS VERIFIED"
echo ""
echo "  ✓ Meta:      greet, skills(N), status, configure"
echo "  ✓ Storage:   upload, download, list, share"
echo "  ✓ Messaging: send, join, create_group"
echo "  ✓ Wallet:    balance, send (gate blocks >500), history"
echo "  ✓ Program:   query, call, deploy"
echo "  ✓ Agent:     card (A2A), discover, task, subscribe, cancel"
echo ""
say "Demo passed: all spec skills work, spending gate enforces thresholds, A2A card is compliant."
# ═══════════════════════════════════════════════════════════════════════
