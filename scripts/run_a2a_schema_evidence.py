#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import pathlib
import shutil
import subprocess
import time
from typing import Any

HOME = pathlib.Path.home()
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = HOME / "lp0008-phase0"
TS = time.strftime("%Y%m%d_%H%M%S", time.gmtime())
BASE = OUT / f"a2a_schema_{TS}"
BASE.mkdir(parents=True, exist_ok=True)
ENV = os.environ.copy()
ENV["PATH"] = f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:" + ENV.get("PATH", "")
ENV["LP0008_DISABLE_WALLET_FFI"] = "1"
ENV["AGENT_MODULE_STATE_DIR"] = str(BASE / "state")
( BASE / "state" ).mkdir(parents=True, exist_ok=True)


def run(name: str, cmd: list[Any], timeout: int = 240, check: bool = True) -> subprocess.CompletedProcess[str]:
    cp = subprocess.run([str(x) for x in cmd], cwd=str(ROOT), env=ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (BASE / f"{name}.log").write_text(cp.stdout)
    print(f"=== {name} rc={cp.returncode} ===")
    print(cp.stdout, end="")
    if check and cp.returncode:
        raise SystemExit(f"{name} failed rc={cp.returncode}")
    return cp


def parse_cabi(text: str) -> Any:
    value = text.strip()
    if value.startswith('"'):
        value = json.loads(value)
    start = value.find("{")
    if start >= 0:
        value = value[start:]
    return json.loads(value)


def call(skill: str, args: list[Any], name: str) -> Any:
    payload = json.dumps([skill, json.dumps(args)])
    cp = run(name, [CALLER, PLUGIN, "dispatchSkill", payload])
    data = parse_cabi(cp.stdout)
    (BASE / f"{name}.json").write_text(json.dumps(data, indent=2, sort_keys=True))
    return data


def require(cond: bool, label: str, detail: Any) -> None:
    if not cond:
        raise AssertionError(f"{label}: {json.dumps(detail, indent=2, sort_keys=True)[:4000]}")


def validate_agent_card(card: dict[str, Any]) -> list[str]:
    assertions: list[str] = []
    required_strings = ["name", "description", "version", "protocol", "protocolVersion", "status", "agent_id", "discovery_topic", "task_topic", "owner_topic"]
    for field in required_strings:
        require(isinstance(card.get(field), str) and card.get(field), f"Agent Card field {field}", card)
        assertions.append(f"Agent Card has non-empty string field {field}")
    require(card["protocol"] == "a2a", "Agent Card protocol", card)
    require(card["protocolVersion"] == "1.0", "Agent Card protocolVersion", card)
    require(card["status"] == "active", "Agent Card active status", card)
    require(card["task_topic"].startswith("/lp0008/1/"), "Agent Card LIP-23 task topic", card)
    require(card["owner_topic"].startswith("/lp0008/1/"), "Agent Card LIP-23 owner topic", card)
    auth = card.get("authentication")
    require(isinstance(auth, dict) and auth.get("type") == "logos_identity", "Agent Card authentication", auth)
    payment = card.get("payment")
    require(isinstance(payment, dict), "Agent Card payment object", card)
    for field in ["currency", "per_task", "recipient", "mechanism"]:
        require(field in payment, f"Agent Card payment field {field}", payment)
    require(payment["currency"] == "LEZ" and payment["mechanism"] == "lez_transfer", "Agent Card LEZ payment mechanism", payment)
    skills = card.get("skills")
    require(isinstance(skills, list) and len(skills) >= 23, "Agent Card skills list", card)
    skill_names = {s.get("name") for s in skills if isinstance(s, dict)}
    for skill in ["agent.card", "agent.discover", "agent.task", "agent.complete", "agent.subscribe", "agent.cancel", "messaging.send"]:
        require(skill in skill_names, f"Agent Card advertises {skill}", skill_names)
    assertions.extend([
        "Agent Card uses protocol=a2a and protocolVersion=1.0",
        "Agent Card advertises Logos identity authentication",
        "Agent Card advertises LEZ payment mechanism",
        "Agent Card advertises A2A lifecycle and messaging skills",
    ])
    return assertions


def validate_task(task: dict[str, Any], expected_topic: str) -> list[str]:
    assertions: list[str] = []
    require(task.get("status") == "completed", "task completed", task)
    require(isinstance(task.get("task_id"), str) and task["task_id"].startswith("task-"), "task_id shape", task)
    require(task.get("agent_address") == "schema-agent", "task target agent", task)
    require(task.get("skill") == "messaging.send", "task skill", task)
    events = [e.get("status") for e in task.get("events", []) if isinstance(e, dict)]
    require(events[:4] == ["queued", "transport_sent", "working", "completed"], "task lifecycle events", task)
    transport = task.get("transport")
    require(isinstance(transport, dict), "task transport object", task)
    require(transport.get("topic") == expected_topic, "transport topic", transport)
    result = transport.get("result")
    require(isinstance(result, dict) and result.get("sent") is True, "transport result sent", result)
    require(result.get("recipient") == expected_topic, "transport recipient topic", result)
    payload = result.get("message")
    if payload is None and result.get("message_id"):
        # messaging.send returns compact transport metadata. Validate the exact
        # persisted outbox/inbox record for the emitted A2A envelope by message_id.
        messages_path = pathlib.Path(ENV["AGENT_MODULE_STATE_DIR"]) / "messages.json"
        messages = json.loads(messages_path.read_text())
        for message in messages:
            if message.get("message_id") == result.get("message_id"):
                payload = message.get("message")
                break
    if isinstance(payload, str):
        envelope = json.loads(payload)
    else:
        envelope = payload
    require(isinstance(envelope, dict), "a2a.task.request envelope object", {"payload": payload, "transport_result": result})
    require(envelope.get("type") == "a2a.task.request", "a2a.task.request type", envelope)
    require(envelope.get("task_id") == task["task_id"], "envelope task_id binding", envelope)
    require(envelope.get("from") == "schema-agent", "envelope from", envelope)
    require(envelope.get("to") == "schema-agent", "envelope to", envelope)
    require(envelope.get("skill") == "messaging.send", "envelope skill", envelope)
    require(isinstance(envelope.get("created_at"), int), "envelope created_at", envelope)
    params = envelope.get("params")
    require(isinstance(params, dict) and params.get("recipient") == "/lp0008/1/a2a-schema/result", "envelope params", envelope)
    assertions.extend([
        "task lifecycle emits queued -> transport_sent -> working -> completed",
        "transport publishes an a2a.task.request envelope to the discovered task_topic",
        "envelope task_id/from/to/skill/params bind to the completed task",
    ])
    return assertions


run("build-install", ["nix", "build", ".#install", "--out-link", "result"], timeout=600)
CALLER = BASE / "cabi_call"
PLUGIN = ROOT / "result/modules/agent_module/agent_module_plugin.dylib"
if not PLUGIN.exists():
    PLUGIN = ROOT / "result/modules/agent_module/agent_module_plugin.so"
run("build-cabi-caller", ["nix", "develop", "--command", "c++", "-std=c++17", "-O2", "-o", CALLER, "tests/cabi_call.cpp"], timeout=600)

call("meta.configure", ["agent_id", "schema-agent"], "config-agent-id")
call("meta.configure", ["agent_name", "Schema Validation Agent"], "config-agent-name")
call("meta.configure", ["description", "LP-0008 A2A schema validation agent"], "config-description")
call("meta.configure", ["task_topic", "/lp0008/1/a2a-schema/tasks"], "config-task-topic")
call("meta.configure", ["owner_topic", "/lp0008/1/a2a-schema/owner"], "config-owner-topic")
call("meta.configure", ["a2a_payment_recipient", "PublicSchemaRecipientForValidationOnly"], "config-payment-recipient")
call("meta.configure", ["a2a_payment_amount_le16", "01000000000000000000000000000000"], "config-payment-amount")
card = call("agent.card", [], "agent-card")
params = {"recipient": "/lp0008/1/a2a-schema/result", "message": "schema validation ping"}
task = call("agent.task", ["schema-agent", "messaging.send", json.dumps(params)], "agent-task")
discovery = call("agent.discover", ["/logos/agents/v1/discovery"], "agent-discover")

assertions = []
assertions += validate_agent_card(card)
assertions += validate_task(task, "/lp0008/1/a2a-schema/tasks")
require(discovery.get("count", 0) >= 1, "discovery count", discovery)
require(any(a.get("agent_id") == "schema-agent" for a in discovery.get("agents", [])), "discovery includes schema-agent", discovery)
assertions.append("discovery registry returns the schema-agent Agent Card")

summary = {
    "schema": "lp0008-a2a-schema-evidence-v1",
    "repo_sha": run("git-sha", ["git", "rev-parse", "HEAD"]).stdout.strip(),
    "command_line": "scripts/run_a2a_schema_evidence.py",
    "evidence_dir": str(BASE),
    "agent_id": card["agent_id"],
    "task_id": task["task_id"],
    "task_topic": card["task_topic"],
    "protocol": card["protocol"],
    "protocolVersion": card["protocolVersion"],
    "skill_count": len(card["skills"]),
    "task_events": [e.get("status") for e in task.get("events", [])],
    "assertions": assertions,
}
summary_path = BASE / "a2a_schema_summary.json"
summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True))
shutil.copy2(summary_path, ROOT / "docs" / "a2a-schema-summary-latest.json")
(ROOT / "docs" / "a2a-schema-evidence.md").write_text("""# A2A schema evidence

This non-video gate validates the LP-0008 Agent Card and task lifecycle envelope shape.

- Script: `scripts/run_a2a_schema_evidence.py`
- Latest summary: `docs/a2a-schema-summary-latest.json`
- Protocol: `a2a`
- Protocol version: `1.0`
- Envelope type: `a2a.task.request`
- Lifecycle: `queued -> transport_sent -> working -> completed`

Strict boundary: this gate validates Agent Card and task lifecycle schema shape. Live Logos Messaging transport is separately proven by `scripts/run_lp0008_deep_verify.py` and `scripts/run_paid_a2a_live_evidence.py`; live LEZ payment binding remains separately blocked by rc3 wallet private-send proof panic.
""")
print(json.dumps({"status": "ok", "summary": str(summary_path), "task_id": task["task_id"], "skill_count": len(card["skills"])}, indent=2, sort_keys=True))
print("A2A_SCHEMA_EVIDENCE_OK")
