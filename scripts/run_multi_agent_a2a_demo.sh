#!/usr/bin/env bash
# LP-0008 multi-agent A2A smoke demo.
#
# This exercises the agent's file-backed A2A control plane: three configured
# identities publish Agent Cards, discovery returns all cards, Alpha drops an
# A2A task envelope onto Beta's task topic, and Beta processes its inbound inbox
# through the persisted queued->working->completed lifecycle. Live delivery send
# remains covered by run_logoscore_integration.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

STATE_ROOT=${LP0008_MULTI_AGENT_STATE:-$HOME/lp0008-phase0/multi_agent_a2a_$(date +%s)}
CALLER="$STATE_ROOT/cabi_call"
PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.dylib"

mkdir -p "$STATE_ROOT"
export AGENT_MODULE_STATE_DIR="$STATE_ROOT"

nix build .#install --out-link result >/dev/null
nix develop --command c++ -std=c++17 -O2 -o "$CALLER" tests/cabi_call.cpp >/dev/null

json_call_args() {
  python3 - "$1" "$2" <<'PY'
import json, sys
print(json.dumps([sys.argv[1], sys.argv[2]]))
PY
}

call_skill() {
  local skill=$1
  local args=$2
  local out=$3
  "$CALLER" "$PLUGIN" dispatchSkill "$(json_call_args "$skill" "$args")" | tee "$out"
}

configure_agent() {
  local id=$1
  local name=$2
  local role=$3
  call_skill meta.configure "[\"agent_id\",\"$id\"]" "$STATE_ROOT/config-$id-id.out" >/dev/null
  call_skill meta.configure "[\"agent_name\",\"$name\"]" "$STATE_ROOT/config-$id-name.out" >/dev/null
  call_skill meta.configure "[\"description\",\"$role\"]" "$STATE_ROOT/config-$id-description.out" >/dev/null
  call_skill meta.configure "[\"task_topic\",\"/lp0008/1/$id/tasks\"]" "$STATE_ROOT/config-$id-task-topic.out" >/dev/null
  call_skill meta.configure "[\"owner_topic\",\"/lp0008/1/$id/owner\"]" "$STATE_ROOT/config-$id-owner-topic.out" >/dev/null
  call_skill agent.card '[]' "$STATE_ROOT/card-$id.out" >/dev/null
}

configure_agent alpha-storage "Alpha Storage" "Storage specialist agent for autonomous file vault tasks"
configure_agent beta-messaging "Beta Messaging" "Messaging specialist agent for owner and peer coordination"
configure_agent gamma-chain "Gamma Chain" "Blockchain specialist agent for wallet and program tasks"

call_skill agent.discover '["/logos/agents/v1/discovery"]' "$STATE_ROOT/discover.out"
configure_agent beta-messaging "Beta Messaging" "Messaging specialist agent for owner and peer coordination" >/dev/null
INBOUND_PAYLOAD=$(python3 - <<'PY'
import json
print(json.dumps({"from":"alpha-storage","skill":"messaging.send","params":{"recipient":"/lp0008/1/alpha-storage/tasks","message":"Alpha delegated a status ping"}}))
PY
)
call_skill messaging.send "$(python3 - "$INBOUND_PAYLOAD" <<'PY'
import json, sys
print(json.dumps(["/lp0008/1/beta-messaging/tasks", sys.argv[1]]))
PY
)" "$STATE_ROOT/inbound-alpha-to-beta.out"
call_skill agent.receive '[]' "$STATE_ROOT/receive-beta.out"
TASK_ID=$(python3 - "$STATE_ROOT/receive-beta.out" <<'PY'
import json, pathlib, sys
received=json.loads(json.loads(pathlib.Path(sys.argv[1]).read_text()))
print(received['tasks'][0]['task_id'])
PY
)
call_skill agent.subscribe "[\"beta-messaging\",\"$TASK_ID\"]" "$STATE_ROOT/subscribe-alpha-to-beta.out"
call_skill agent.cancel "[\"beta-messaging\",\"$TASK_ID\"]" "$STATE_ROOT/cancel-alpha-to-beta.out"

python3 - "$STATE_ROOT" <<'PY'
import json, pathlib, sys
base=pathlib.Path(sys.argv[1])
def parse(name):
    return json.loads(json.loads((base/name).read_text()))
discovery=parse('discover.out')
ids={a.get('agent_id') for a in discovery.get('agents', [])}
expected={'alpha-storage','beta-messaging','gamma-chain'}
assert expected.issubset(ids), ids
for card in discovery['agents']:
    if card['agent_id'] in expected:
        assert card.get('protocol') == 'a2a', card
        assert card.get('task_topic','').startswith('/lp0008/1/'), card
        assert len(card.get('skills', [])) >= 20, card

inbound=parse('inbound-alpha-to-beta.out')
received=parse('receive-beta.out')
task=received['tasks'][0]
sub=parse('subscribe-alpha-to-beta.out')
cancel=parse('cancel-alpha-to-beta.out')
assert inbound['sent'] is True, inbound
assert received['processed'] >= 1, received
assert task['agent_address'] == 'alpha-storage' and task['skill'] == 'messaging.send', task
assert task['status'] == 'completed', task
assert [e.get('status') for e in task.get('events', [])] == ['queued','working','completed'], task
assert task.get('result', {}).get('sent') is True, task
assert sub['subscribed'] is True and sub['task_id'] == task['task_id'], sub
assert sub.get('current_status') == 'completed', sub
assert cancel['cancelled'] is False and cancel['task_id'] == task['task_id'], cancel
assert cancel.get('current_status') == 'completed', cancel
print('ASSERT multi-agent A2A: 3 cards discovered; inbound task envelope processed queued->working->completed; subscribe observes result; completed task is not cancelled: OK')
print(f'EVIDENCE_DIR={base}')
PY
