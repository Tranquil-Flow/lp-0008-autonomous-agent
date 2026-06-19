#!/usr/bin/env python3
"""Live LP-0008 wallet-send verifier."""
from __future__ import annotations

import json
import os
import pathlib
import shutil
import subprocess
import time
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_WALLET_CLI = pathlib.Path.home() / "Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet"
DEFAULT_STATE = pathlib.Path.home() / "lp0008-phase0/live_wallet_send_verify"
DEFAULT_AMOUNT = "01000000000000000000000000000000"
SEQ = "https://testnet.lez.logos.co/"


def need_env(name: str) -> str:
    value = os.environ.get(name, "").strip()
    if not value:
        raise SystemExit(f"missing required env {name}")
    return value


def run(cmd: list[str], *, env: dict[str, str] | None = None, timeout: int = 180, check: bool = True) -> subprocess.CompletedProcess[str]:
    cp = subprocess.run(cmd, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    if check and cp.returncode != 0:
        raise SystemExit(f"command failed ({cp.returncode}): {' '.join(cmd)}\n{cp.stdout}")
    return cp


def parse_cabi(stdout: str) -> Any:
    text = stdout.strip()
    starts = [i for i in (text.find('"'), text.find('{'), text.find('[')) if i >= 0]
    if starts:
        text = text[min(starts):]
    try:
        if text.startswith('"'):
            text = json.loads(text)
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise SystemExit(f"could not parse cabi stdout: {stdout[:500]!r}") from exc


def call_skill(plugin: pathlib.Path, caller: pathlib.Path, state: pathlib.Path, skill: str, args: list[str]) -> Any:
    env = os.environ.copy()
    env["AGENT_MODULE_STATE_DIR"] = str(state)
    payload = json.dumps([skill, json.dumps(args)])
    cp = run([str(caller), str(plugin), "dispatchSkill", payload], env=env, timeout=600)
    return parse_cabi(cp.stdout)


def wallet_cli(wallet: pathlib.Path, wallet_home: pathlib.Path, args: list[str], timeout: int = 180, check: bool = True) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env["NSSA_WALLET_HOME_DIR"] = str(wallet_home)
    return run([str(wallet), *args], env=env, timeout=timeout, check=check)


def main() -> None:
    wallet_home = pathlib.Path(need_env("LP0008_LIVE_WALLET_HOME")).expanduser().resolve()
    account = need_env("LP0008_LIVE_WALLET_ACCOUNT")
    recipient = need_env("LP0008_LIVE_SEND_RECIPIENT")
    amount = os.environ.get("LP0008_LIVE_SEND_AMOUNT_LE16", DEFAULT_AMOUNT)
    wallet = pathlib.Path(os.environ.get("LP0008_RC3_WALLET_CLI", str(DEFAULT_WALLET_CLI))).expanduser().resolve()
    state = pathlib.Path(os.environ.get("LP0008_LIVE_AGENT_STATE", str(DEFAULT_STATE))).expanduser().resolve()

    if not wallet.exists():
        raise SystemExit(f"wallet CLI not found: {wallet}")
    if not (wallet_home / "storage.json").exists() or not (wallet_home / "wallet_config.json").exists():
        raise SystemExit(f"wallet home must contain storage.json and wallet_config.json: {wallet_home}")

    plugin = ROOT / "result/modules/agent_module/agent_module_plugin.dylib"
    caller = ROOT / ".demo-state/cabi_call"
    if not plugin.exists() or not caller.exists():
        raise SystemExit("missing built plugin or .demo-state/cabi_call; run nix build and demo/deep verify first")

    if state.exists():
        shutil.rmtree(state)
    state.mkdir(parents=True)
    shutil.copytree(wallet_home, state / "wallet")
    (state / "config.json").write_text(json.dumps({
        "wallet_account_hex": account,
        "sequencer_addr": SEQ,
        "per_tx_limit": "1000",
    }, indent=2))

    before = call_skill(plugin, caller, state, "wallet.balance", [])
    if before.get("account") != account or before.get("mode") != "live":
        raise SystemExit(f"agent did not mount intended live account: {before}")
    before_balance = int(before.get("balance", "0"))
    if before_balance <= 0:
        raise SystemExit(f"funded account has non-positive balance: {before}")

    send = call_skill(plugin, caller, state, "wallet.send", [recipient, amount])
    if not send.get("submitted") or not send.get("tx_hash"):
        raise SystemExit(f"wallet.send did not submit: {send}")
    tx_hash = send["tx_hash"]

    seen = False
    tx_excerpt = ""
    for _ in range(18):
        cp = wallet_cli(wallet, state / "wallet", ["chain-info", "transaction", "--hash", tx_hash], timeout=120, check=False)
        tx_excerpt = cp.stdout[:2000]
        if "Transaction is Some" in cp.stdout:
            seen = True
            break
        time.sleep(10)
    if not seen:
        raise SystemExit(f"submitted tx not found after polling: {tx_hash}\n{tx_excerpt}")

    wallet_cli(wallet, state / "wallet", ["account", "sync-private"], timeout=180, check=False)
    after = call_skill(plugin, caller, state, "wallet.balance", [])
    after_balance = int(after.get("balance", "0"))
    if after_balance >= before_balance:
        raise SystemExit(json.dumps({"error": "balance_not_decreased", "before": before, "after": after, "tx_hash": tx_hash}, indent=2))

    print(json.dumps({
        "ok": True,
        "sequencer": SEQ,
        "account": account,
        "recipient": recipient,
        "amount_le16": amount,
        "before_balance": before_balance,
        "after_balance": after_balance,
        "tx_hash": tx_hash,
        "tx_found": True,
        "state": str(state),
    }, indent=2))


if __name__ == "__main__":
    main()
