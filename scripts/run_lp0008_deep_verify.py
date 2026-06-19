#!/usr/bin/env python3
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time

HOME = pathlib.Path.home()
ROOT = HOME / "Projects" / "logos-basecamp"
AGENT = ROOT / "lp-0008-autonomous-agent"
STORAGE = ROOT / "refs" / "logos-storage-module"
DELIVERY = ROOT / "refs" / "logos-delivery-module"
OUT_ROOT = HOME / "lp0008-phase0"
BASE = OUT_ROOT / f"deep_verify_{int(time.time())}"
BASE.mkdir(parents=True, exist_ok=True)

ENV = os.environ.copy()
ENV["PATH"] = f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:" + ENV.get("PATH", "")

def run_cmd(name, cmd, cwd=None, check=True, timeout=240):
    print(f"\n=== {name} ===")
    print("$ " + " ".join(map(str, cmd)))
    cp = subprocess.run([str(x) for x in cmd], cwd=str(cwd) if cwd else None, env=ENV,
                        text=True, capture_output=True, timeout=timeout)
    (BASE / f"{name}.stdout").write_text(cp.stdout)
    (BASE / f"{name}.stderr").write_text(cp.stderr)
    print(cp.stdout, end="")
    if cp.stderr:
        print(cp.stderr, end="", file=sys.stderr)
    print(f"[exit {cp.returncode}]")
    if check and cp.returncode != 0:
        raise SystemExit(f"{name} failed rc={cp.returncode}")
    return cp

# Build from current source, not stale symlinks.
run_cmd("build-agent", ["nix", "build", ".#install", "--print-out-paths"], cwd=AGENT, timeout=600)
run_cmd("build-storage", ["nix", "build", ".#install", "--print-out-paths"], cwd=STORAGE, timeout=600)
run_cmd("build-delivery", ["nix", "build", ".#install", "--print-out-paths"], cwd=DELIVERY, timeout=600)

cli = OUT_ROOT / "logoscore-cli" / "bin" / "logoscore"
if not cli.exists():
    run_cmd("build-logoscore-cli", ["nix", "build", "github:logos-co/logos-logoscore-cli#default", "--out-link", str(OUT_ROOT / "logoscore-cli")], timeout=600)

agent_mods = AGENT / "result" / "modules"
storage_mods = STORAGE / "result" / "modules"
delivery_mods = DELIVERY / "result" / "modules"
for path in [agent_mods / "agent_module" / "manifest.json", storage_mods / "storage_module" / "manifest.json", delivery_mods / "delivery_module" / "manifest.json"]:
    if not path.exists():
        raise SystemExit(f"missing module manifest {path}")

session = BASE / "logoscore-session"
(session / "config").mkdir(parents=True)
(session / "persist").mkdir(parents=True)
log = open(session / "daemon.log", "w")
daemon = subprocess.Popen([
    str(cli), "--config-dir", str(session / "config"),
    "-m", str(storage_mods), "-m", str(delivery_mods), "-m", str(agent_mods),
    "--persistence-path", str(session / "persist"), "--persist-config", "daemon"
], stdout=log, stderr=subprocess.STDOUT, env=ENV, text=True)
(session / "daemon.pid").write_text(str(daemon.pid))

def cli_cmd(name, args, check=True):
    return run_cmd(name, [cli, "--config-dir", session / "config", *args], check=check, timeout=180)

def parse_json_file(name):
    return json.loads((BASE / f"{name}.stdout").read_text())

def parse_skill_result(name):
    outer = parse_json_file(name)
    if outer.get("status") != "ok":
        raise AssertionError((name, outer))
    result = outer.get("result")
    if isinstance(result, str):
        return json.loads(result)
    return result

try:
    for _ in range(100):
        cp = subprocess.run([str(cli), "--config-dir", str(session / "config"), "list-modules"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=ENV)
        if cp.returncode == 0:
            break
        time.sleep(0.25)
    else:
        raise SystemExit("logoscore daemon did not become ready")

    cli_cmd("load-storage", ["load-module", "storage_module"])
    cli_cmd("load-delivery", ["load-module", "delivery_module"])
    cli_cmd("load-agent", ["load-module", "agent_module"])
    cli_cmd("list-modules-after-load", ["list-modules"])

    cli_cmd("meta-skills", ["call", "agent_module", "dispatchSkill", "meta.skills", "[]"])
    skills = parse_skill_result("meta-skills")
    assert isinstance(skills, list) and len(skills) >= 23, len(skills)
    names = {item.get("name") for item in skills}
    assert "agent.complete" in names, skills
    assert "agent.receive" in names, skills

    cli_cmd("meta-status-before", ["call", "agent_module", "dispatchSkill", "meta.status", "[]"])
    status = parse_skill_result("meta-status-before")
    assert status["modules"]["storage_module"] in {"connected", "not_loaded"}, status
    assert status["modules"]["delivery_module"] in {"connected", "not_loaded"}, status
    assert status["wallet"]["mode"] in {"live", "simulated"}, status

    cli_cmd("wallet-balance", ["call", "agent_module", "dispatchSkill", "wallet.balance", "[]"])
    balance = parse_skill_result("wallet-balance")
    assert balance.get("mode") == "live" and balance.get("account") and balance.get("sequencer"), balance
    assert status["wallet"].get("account") == balance.get("account"), {"status": status, "balance": balance}
    cli_cmd("wallet-history", ["call", "agent_module", "dispatchSkill", "wallet.history", "[]"])
    history = parse_skill_result("wallet-history")
    assert "transactions" in history and isinstance(history["transactions"], list), history

    input_file = session / "manual-input.txt"
    input_file.write_text("LP-0008 deep verification payload moonlit and real\n")
    cli_cmd("storage-upload", ["call", "agent_module", "dispatchSkill", "storage.upload", json.dumps([str(input_file), "deep-manual-file"])] )
    upload = parse_skill_result("storage-upload")
    assert upload.get("stored") is True and upload.get("label") == "deep-manual-file", upload
    assert upload.get("mode") == "live", upload

    cli_cmd("storage-list", ["call", "agent_module", "dispatchSkill", "storage.list", "[]"])
    listing = parse_skill_result("storage-list")
    assert listing.get("count", 0) >= 1 and any(f.get("label") == "deep-manual-file" for f in listing.get("files", [])), listing
    manifests = listing.get("storage_result", {}).get("value", [])
    assert any(m.get("cid") == upload["address"] and m.get("datasetSize") == len(input_file.read_text()) for m in manifests), listing
    storage_jsons = list((session / "persist").rglob("storage.json"))
    assert storage_jsons, "no storage.json persisted"
    for p in storage_jsons:
        shutil.copy2(p, BASE / f"persist-{p.parent.name}-storage.json")

    download_path = session / "manual-download.txt"
    cli_cmd("storage-download", ["call", "agent_module", "dispatchSkill", "storage.download", json.dumps([upload["address"], str(download_path)])])
    download = parse_skill_result("storage-download")
    assert download.get("mode") == "live", download
    assert download.get("downloaded") is True and download_path.exists(), download
    assert download_path.read_text() == input_file.read_text(), (download, download_path.read_text())

    cli_cmd("messaging-valid", ["call", "agent_module", "dispatchSkill", "messaging.send", json.dumps(["/lp0008/1/deepverify/proto", "deep verification live delivery ping"])] )
    msg = parse_skill_result("messaging-valid")
    assert msg.get("sent") is True and msg.get("mode") == "live" and msg.get("message_id"), msg

    cli_cmd("messaging-invalid", ["call", "agent_module", "dispatchSkill", "messaging.send", json.dumps(["/logos/lp0008/not-lip23", "should not reach delivery"])] )
    bad = parse_skill_result("messaging-invalid")
    assert bad.get("sent") is True and bad.get("mode") == "simulated", bad
    cli_cmd("list-modules-after-invalid", ["list-modules"])
    after_bad = parse_json_file("list-modules-after-invalid")
    loaded = {m["name"]: m["status"] for m in after_bad}
    assert loaded.get("delivery_module") == "loaded", loaded

    cli_cmd("missing-file-negative", ["call", "agent_module", "dispatchSkill", "storage.upload", json.dumps([str(session / "does-not-exist.txt"), "missing"])] )
    missing = parse_skill_result("missing-file-negative")
    assert missing.get("error") == "file_not_found", missing

    cli_cmd("spending-threshold-negative", ["call", "agent_module", "dispatchSkill", "wallet.send", json.dumps(["invalid-recipient-for-threshold-check", "999999999999"])] )
    spend = parse_skill_result("spending-threshold-negative")
    assert spend.get("approved") is False or spend.get("error"), spend

    cli_cmd("config-agent-id", ["call", "agent_module", "dispatchSkill", "meta.configure", json.dumps(["agent_id", "deep-verify-agent"])] )
    cli_cmd("config-agent-name", ["call", "agent_module", "dispatchSkill", "meta.configure", json.dumps(["agent_name", "Deep Verify Agent"])] )
    cli_cmd("agent-card", ["call", "agent_module", "dispatchSkill", "agent.card", "[]"])
    card = parse_skill_result("agent-card")
    assert card.get("agent_id") == "deep-verify-agent" and card.get("name") == "Deep Verify Agent", card
    assert len(card.get("skills", [])) >= 23, card
    card_names = {item.get("name") for item in card.get("skills", [])}
    assert "agent.complete" in card_names, card
    assert "agent.receive" in card_names, card
    cli_cmd("agent-discover", ["call", "agent_module", "dispatchSkill", "agent.discover", json.dumps(["/logos/agents/v1/discovery"])] )
    disc = parse_skill_result("agent-discover")
    assert any(a.get("agent_id") == "deep-verify-agent" for a in disc.get("agents", [])), disc

    cli_cmd("config-task-topic", ["call", "agent_module", "dispatchSkill", "meta.configure", json.dumps(["task_topic", "/lp0008/1/deepverify/tasks"])] )
    inbound_payload = json.dumps({"from":"alpha", "skill":"messaging.send", "params":{"recipient":"/lp0008/1/deepverify/proto", "message":"inbound task ping"}})
    cli_cmd("inbound-task-message", ["call", "agent_module", "dispatchSkill", "messaging.send", json.dumps(["/lp0008/1/deepverify/tasks", inbound_payload])] )
    inbound_msg = parse_skill_result("inbound-task-message")
    assert inbound_msg.get("sent") is True, inbound_msg
    cli_cmd("agent-receive", ["call", "agent_module", "dispatchSkill", "agent.receive", "[]"])
    received = parse_skill_result("agent-receive")
    assert received.get("processed") >= 1, received
    assert any(item.get("status") == "completed" and item.get("result", {}).get("sent") is True for item in received.get("tasks", [])), received

    cli_cmd("agent-task", ["call", "agent_module", "dispatchSkill", "agent.task", json.dumps(["deep-verify-agent", "messaging.send", '{"recipient":"/lp0008/1/deepverify/proto","message":"task ping"}'])] )
    task = parse_skill_result("agent-task")
    assert task.get("status") == "completed" and task.get("task_id"), task
    assert task.get("result", {}).get("sent") is True and task.get("result", {}).get("mode") == "live", task
    cli_cmd("agent-subscribe", ["call", "agent_module", "dispatchSkill", "agent.subscribe", json.dumps(["deep-verify-agent", task["task_id"]])] )
    sub = parse_skill_result("agent-subscribe")
    assert sub.get("subscribed") is True and sub.get("current_status") == "completed", sub
    assert sub.get("result", {}).get("sent") is True, sub

    cli_cmd("agent-task-failure", ["call", "agent_module", "dispatchSkill", "agent.task", json.dumps(["deep-verify-agent", "agent.no_such_skill", '{}'])] )
    failed = parse_skill_result("agent-task-failure")
    assert failed.get("status") == "failed" and failed.get("error", {}).get("error") == "unknown_skill", failed

    cli_cmd("agent-complete-manual", ["call", "agent_module", "dispatchSkill", "agent.complete", json.dumps([failed["task_id"], '{"manual":"override"}'])] )
    completed = parse_skill_result("agent-complete-manual")
    assert completed.get("completed") is True and completed.get("status") == "completed", completed
    assert completed.get("result", {}).get("manual") == "override", completed

    cli_cmd("agent-cancel-completed", ["call", "agent_module", "dispatchSkill", "agent.cancel", json.dumps(["deep-verify-agent", task["task_id"]])] )
    cancel = parse_skill_result("agent-cancel-completed")
    assert cancel.get("cancelled") is False and cancel.get("current_status") == "completed", cancel

    status_after_tasks = parse_skill_result("meta-status-before")
    task_jsons = list((session / "persist").rglob("tasks.json"))
    assert task_jsons, "no tasks.json persisted"
    for p in task_jsons:
        raw_tasks = json.loads(p.read_text())
        by_id = {t.get("task_id"): t for t in raw_tasks}
        persisted = by_id.get(task["task_id"])
        if persisted:
            assert persisted.get("status") == "completed", persisted
            assert [e.get("status") for e in persisted.get("events", [])] == ["queued", "working", "completed"], persisted
            assert persisted.get("result", {}).get("sent") is True, persisted
        shutil.copy2(p, BASE / f"persist-{p.parent.name}-tasks.json")

    # Cross-instance / C ABI multi-agent verification, with persisted files inspected afterward.
    multi_base = OUT_ROOT / f"deep_verify_multi_agent_{int(time.time())}"
    cp = run_cmd("multi-agent-cabi-script", ["bash", "scripts/run_multi_agent_a2a_demo.sh"], cwd=AGENT, check=True, timeout=600)
    # The script chooses its own evidence dir; parse it from stdout.
    evidence = None
    for line in cp.stdout.splitlines():
        if line.startswith("EVIDENCE_DIR="):
            evidence = pathlib.Path(line.split("=",1)[1].strip())
    assert evidence and evidence.exists(), evidence
    for fname in ["discovery.json", "tasks.json", "config.json"]:
        matches = list(evidence.rglob(fname))
        for p in matches:
            shutil.copy2(p, BASE / f"multi-{fname}")
    discovery_path = evidence / "discovery.json"
    tasks_path = evidence / "tasks.json"
    assert discovery_path.exists() and tasks_path.exists(), list(evidence.iterdir())
    discovery = json.loads(discovery_path.read_text())
    tasks = json.loads(tasks_path.read_text())
    ids = {a.get("agent_id") for a in discovery}
    assert {"alpha-storage","beta-messaging","gamma-chain"}.issubset(ids), ids
    assert any(t.get("status") == "completed" and t.get("subscribed") is True and t.get("result", {}).get("sent") is True and [e.get("status") for e in t.get("events", [])] == ["queued", "working", "completed"] for t in tasks), tasks

    print("\nDEEP_VERIFY_PASS=1")
    print(f"EVIDENCE_DIR={BASE}")
finally:
    if daemon.poll() is None:
        daemon.terminate()
        try:
            daemon.wait(timeout=5)
        except subprocess.TimeoutExpired:
            daemon.kill()
    log.close()
