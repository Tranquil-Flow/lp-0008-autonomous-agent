#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

STATE_ROOT=${LP0008_USE_CASE_STATE:-$HOME/lp0008-phase0/three_use_cases_$(date +%s)}
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

# Use case 1: personal file vault — upload, download, list, share.
VAULT_IN="$STATE_ROOT/vault-secret.txt"
VAULT_OUT="$STATE_ROOT/vault-secret-restored.txt"
printf 'LP-0008 vault secret %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$VAULT_IN"
call_skill storage.upload "$(json_array "$VAULT_IN" personal-vault)" "$STATE_ROOT/usecase1-upload.out" >/dev/null
ADDR=$(python3 - "$STATE_ROOT/usecase1-upload.out" <<'PY'
import json, pathlib, sys
print(json.loads(json.loads(pathlib.Path(sys.argv[1]).read_text()))['address'])
PY
)
call_skill storage.download "$(json_array "$ADDR" "$VAULT_OUT")" "$STATE_ROOT/usecase1-download.out" >/dev/null
call_skill storage.list '[]' "$STATE_ROOT/usecase1-list.out" >/dev/null
call_skill storage.share "$(json_array "$ADDR" beta-messaging)" "$STATE_ROOT/usecase1-share.out" >/dev/null
cmp "$VAULT_IN" "$VAULT_OUT"

# Use case 2: agent services marketplace — publish service card, delegate task, pay per task.
call_skill meta.configure '["agent_id","marketplace-buyer"]' "$STATE_ROOT/usecase2-config-id.out" >/dev/null
call_skill meta.configure '["agent_name","Marketplace Buyer"]' "$STATE_ROOT/usecase2-config-name.out" >/dev/null
call_skill meta.configure '["task_topic","/lp0008/1/marketplace-buyer/tasks"]' "$STATE_ROOT/usecase2-config-topic.out" >/dev/null
call_skill meta.configure '["a2a_payment_recipient","yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3"]' "$STATE_ROOT/usecase2-config-recipient.out" >/dev/null
call_skill meta.configure '["a2a_payment_amount_le16","0a000000000000000000000000000000"]' "$STATE_ROOT/usecase2-config-payment.out" >/dev/null
call_skill agent.card '[]' "$STATE_ROOT/usecase2-card.out" >/dev/null
TASK_PARAMS=$(python3 - <<'PY'
import json
print(json.dumps({"recipient":"/lp0008/1/marketplace/service","message":"please provide a status summary"}))
PY
)
call_skill agent.task "$(json_array marketplace-buyer messaging.send "$TASK_PARAMS")" "$STATE_ROOT/usecase2-task.out" >/dev/null
call_skill wallet.history '[]' "$STATE_ROOT/usecase2-history.out" >/dev/null

# Use case 3: multi-agent workflow — Alpha discovers Beta/Gamma, Beta processes delegated task.
call_skill meta.configure '["agent_id","alpha-storage"]' "$STATE_ROOT/usecase3-alpha-id.out" >/dev/null
call_skill meta.configure '["agent_name","Alpha Storage"]' "$STATE_ROOT/usecase3-alpha-name.out" >/dev/null
call_skill meta.configure '["description","Storage specialist agent for personal vault tasks"]' "$STATE_ROOT/usecase3-alpha-desc.out" >/dev/null
call_skill meta.configure '["task_topic","/lp0008/1/alpha-storage/tasks"]' "$STATE_ROOT/usecase3-alpha-topic.out" >/dev/null
call_skill agent.card '[]' "$STATE_ROOT/usecase3-alpha-card.out" >/dev/null
call_skill meta.configure '["agent_id","beta-messaging"]' "$STATE_ROOT/usecase3-beta-id.out" >/dev/null
call_skill meta.configure '["agent_name","Beta Messaging"]' "$STATE_ROOT/usecase3-beta-name.out" >/dev/null
call_skill meta.configure '["description","Messaging specialist agent for peer coordination"]' "$STATE_ROOT/usecase3-beta-desc.out" >/dev/null
call_skill meta.configure '["task_topic","/lp0008/1/beta-messaging/tasks"]' "$STATE_ROOT/usecase3-beta-topic.out" >/dev/null
call_skill agent.card '[]' "$STATE_ROOT/usecase3-beta-card.out" >/dev/null
call_skill meta.configure '["agent_id","gamma-chain"]' "$STATE_ROOT/usecase3-gamma-id.out" >/dev/null
call_skill meta.configure '["agent_name","Gamma Chain"]' "$STATE_ROOT/usecase3-gamma-name.out" >/dev/null
call_skill meta.configure '["description","Blockchain specialist agent for wallet and program tasks"]' "$STATE_ROOT/usecase3-gamma-desc.out" >/dev/null
call_skill meta.configure '["task_topic","/lp0008/1/gamma-chain/tasks"]' "$STATE_ROOT/usecase3-gamma-topic.out" >/dev/null
call_skill agent.card '[]' "$STATE_ROOT/usecase3-gamma-card.out" >/dev/null
call_skill agent.discover '["/logos/agents/v1/discovery"]' "$STATE_ROOT/usecase3-discover.out" >/dev/null
call_skill meta.configure '["agent_id","beta-messaging"]' "$STATE_ROOT/usecase3-beta-active-id.out" >/dev/null
call_skill meta.configure '["task_topic","/lp0008/1/beta-messaging/tasks"]' "$STATE_ROOT/usecase3-beta-active-topic.out" >/dev/null
INBOUND=$(python3 - <<'PY'
import json
print(json.dumps({"from":"alpha-storage","skill":"messaging.send","params":{"recipient":"/lp0008/1/gamma-chain/tasks","message":"workflow handoff from Alpha to Gamma via Beta"}}))
PY
)
call_skill messaging.send "$(json_array /lp0008/1/beta-messaging/tasks "$INBOUND")" "$STATE_ROOT/usecase3-inbound.out" >/dev/null
call_skill agent.receive '[]' "$STATE_ROOT/usecase3-receive.out" >/dev/null

python3 - "$STATE_ROOT" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1])
def parse(name):
    return json.loads(json.loads((base/name).read_text()))
# Use case 1 assertions.
upload=parse('usecase1-upload.out'); download=parse('usecase1-download.out'); listing=parse('usecase1-list.out'); share=parse('usecase1-share.out')
assert upload.get('stored') is True and upload.get('address'), upload
assert download.get('downloaded') is True and pathlib.Path(download['path']).exists(), download
download_size=pathlib.Path(download['path']).stat().st_size
assert download_size > 0, download
assert listing.get('count', 0) >= 1, listing
assert share.get('shared') is True and share.get('recipient') == 'beta-messaging', share
# Use case 2 assertions.
card=parse('usecase2-card.out'); task=parse('usecase2-task.out'); hist=parse('usecase2-history.out')
assert card.get('payment', {}).get('per_task') == '0a000000000000000000000000000000', card.get('payment')
assert task.get('status') == 'completed', task
assert task.get('payment', {}).get('submitted') is True, task
assert 'payment_submitted' in [e.get('status') for e in task.get('events', [])], task
assert any(t.get('tx_hash') == task['payment'].get('tx_hash') for t in hist.get('transactions', [])), hist
# Use case 3 assertions.
disc=parse('usecase3-discover.out'); recv=parse('usecase3-receive.out')
ids={a.get('agent_id') for a in disc.get('agents', [])}
assert {'alpha-storage','beta-messaging','gamma-chain'}.issubset(ids), ids
assert recv.get('processed', 0) >= 1, recv
t=recv['tasks'][0]
assert t.get('agent_address') == 'alpha-storage' and t.get('status') == 'completed', t
assert t.get('result', {}).get('sent') is True, t
summary={
  'use_case_1_personal_file_vault': {'stored': upload['address'], 'downloaded_bytes': download_size, 'shared_with': share['recipient']},
  'use_case_2_agent_services_marketplace': {'task_id': task['task_id'], 'payment_submitted': task['payment']['submitted'], 'payment_mode': task['payment']['mode']},
  'use_case_3_multi_agent_workflow': {'agents': sorted(ids), 'processed': recv['processed'], 'task_status': t['status']},
  'evidence_dir': str(base),
}
(base/'summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True))
print(json.dumps(summary, indent=2, sort_keys=True))
print('ASSERT three illustrative use cases: personal vault, marketplace payment hook, multi-agent workflow: OK')
PY
