#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import pathlib
import subprocess
import time

HOME = pathlib.Path.home()
ROOT = HOME / "Projects" / "logos-basecamp"
AGENT = ROOT / "lp-0008-autonomous-agent"
STORAGE = ROOT / "refs" / "logos-storage-module"
DELIVERY = ROOT / "refs" / "logos-delivery-module"
OUT = HOME / "lp0008-phase0"
TS = time.strftime("%Y%m%d_%H%M%S", time.gmtime())
BASE = OUT / f"headless_deploy_{TS}"
BASE.mkdir(parents=True, exist_ok=True)
ENV = os.environ.copy()
ENV["PATH"] = f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:" + ENV.get("PATH", "")
ENV["LP0008_DISABLE_WALLET_FFI"] = "1"


def run(name: str, cmd: list[object], cwd: pathlib.Path = AGENT, timeout: int = 300) -> subprocess.CompletedProcess[str]:
    print(f"\n=== {name} ===")
    print("$ " + " ".join(map(str, cmd)))
    cp = subprocess.run([str(x) for x in cmd], cwd=str(cwd), env=ENV, text=True,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (BASE / f"{name}.log").write_text(cp.stdout)
    print(cp.stdout, end="")
    print(f"[exit {cp.returncode}]")
    if cp.returncode:
        raise SystemExit(f"{name} failed rc={cp.returncode}")
    return cp


def cli_call(cli: pathlib.Path, config: pathlib.Path, name: str, args: list[object]) -> subprocess.CompletedProcess[str]:
    return run(name, [cli, "--config-dir", config, *args], timeout=180)

single_cli_command = (
    "logoscore --config-dir <config> -m <storage/result/modules> -m <delivery/result/modules> "
    "-m <agent/result/modules> --persistence-path <persist> --persist-config daemon; "
    "logoscore load-module storage_module; logoscore load-module delivery_module; "
    "logoscore load-module agent_module; logoscore call agent_module dispatchSkill meta.configure ..."
)

run("build-agent", ["nix", "build", ".#install", "--out-link", "result"], timeout=600)
run("build-storage", ["nix", "build", ".#install", "--out-link", "result"], cwd=STORAGE, timeout=600)
run("build-delivery", ["nix", "build", ".#install", "--out-link", "result"], cwd=DELIVERY, timeout=600)
cli = OUT / "logoscore-cli" / "bin" / "logoscore"
if not cli.exists():
    run("build-logoscore-cli", ["nix", "build", "github:logos-co/logos-logoscore-cli#default", "--out-link", str(OUT / "logoscore-cli")], timeout=600)

session = BASE / "logoscore-session"
config = session / "config"
persist = session / "persist"
config.mkdir(parents=True)
persist.mkdir(parents=True)
log = open(session / "daemon.log", "w")
daemon = subprocess.Popen([
    str(cli), "--config-dir", str(config),
    "-m", str(STORAGE / "result/modules"),
    "-m", str(DELIVERY / "result/modules"),
    "-m", str(AGENT / "result/modules"),
    "--persistence-path", str(persist), "--persist-config", "daemon",
], stdout=log, stderr=subprocess.STDOUT, env=ENV, text=True)
(session / "daemon.pid").write_text(str(daemon.pid))
try:
    for _ in range(120):
        cp = subprocess.run([str(cli), "--config-dir", str(config), "list-modules"], env=ENV,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if cp.returncode == 0:
            break
        time.sleep(0.25)
    else:
        raise SystemExit("logoscore daemon did not become ready")

    cli_call(cli, config, "load-storage", ["load-module", "storage_module"])
    cli_call(cli, config, "load-delivery", ["load-module", "delivery_module"])
    cli_call(cli, config, "load-agent", ["load-module", "agent_module"])
    for key, value in [
        ("agent_id", "headless-deploy-agent"),
        ("agent_name", "Headless Deploy Evidence Agent"),
        ("owner_topic", "/lp0008/1/headless/owner"),
        ("task_topic", "/lp0008/1/headless/tasks"),
    ]:
        cli_call(cli, config, f"meta.configure-{key}", ["call", "agent_module", "dispatchSkill", "meta.configure", json.dumps([key, value])])
    cli_call(cli, config, "meta.status", ["call", "agent_module", "dispatchSkill", "meta.status", "[]"])
    cli_call(cli, config, "agent.card", ["call", "agent_module", "dispatchSkill", "agent.card", "[]"])
finally:
    daemon.terminate()
    try:
        daemon.wait(timeout=10)
    except subprocess.TimeoutExpired:
        daemon.kill()
    log.close()

status_outer = json.loads((BASE / "meta.status.log").read_text())
status = json.loads(status_outer["result"])
card_outer = json.loads((BASE / "agent.card.log").read_text())
card = json.loads(card_outer["result"])
summary = {
    "ok": True,
    "schema": "lp0008-headless-deploy-evidence-v1",
    "repo_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=AGENT, text=True).strip(),
    "command_line": "scripts/run_headless_deploy_evidence.py",
    "single_cli_command": single_cli_command,
    "environment": {"LP0008_DISABLE_WALLET_FFI": ENV["LP0008_DISABLE_WALLET_FFI"]},
    "loaded_modules": status.get("modules", {}),
    "agent_id": card.get("agent_id"),
    "owner_topic": card.get("owner_topic"),
    "task_topic": card.get("task_topic"),
    "evidence_dir": str(BASE),
    "raw_logs": sorted(str(p) for p in BASE.glob("*.log")),
    "limitations": ["Darwin wallet FFI is disabled for this portable headless deploy proof; wallet live sends remain covered by separate wallet evidence."],
}
(BASE / "headless_deploy_summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
print("HEADLESS_DEPLOY_EVIDENCE_OK " + str(BASE / "headless_deploy_summary.json"))
