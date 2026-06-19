#!/usr/bin/env python3
"""Reviewer-facing LP-0008 acceptance readiness validator.

This is deliberately stricter than the demo: it guards public claims, dependency
policy, and known upstream receive limitations so the submission cannot drift
into overclaiming.
"""
from __future__ import annotations
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_FILES = ["README.md", "SUBMISSION.md", "VIDEO_SCRIPT.md", "HANDOVER.md"]
REQUIRED_FILES = [
    "docs/dependency-policy.md",
    "docs/upstream/delivery-receive-poll-api.md",
    "docs/acceptance-gap-audit.md",
    "docs/upstream/program-live-api.md",
]
FORBIDDEN_PATTERNS = [
    (re.compile(r"\b21 methods\b", re.I), "stale 21-method wording"),
    (re.compile(r"\b21 skills\b|\b22 skills\b", re.I), "stale skill count"),
    (re.compile(r"LogosAPI class is not available|modules\(\) returns empty", re.I), "obsolete LogosAPI limitation"),
    (re.compile(r"wallet operations return simulated|Skills return simulated data", re.I), "obsolete blanket simulation claim"),
    (re.compile(r"Requires LEZ testnet access; not available|No testnet access", re.I), "obsolete testnet-access claim"),
    (re.compile(r"Delivery module has incompatible API", re.I), "imprecise delivery limitation"),
    (re.compile(r"\[Link to be added\]", re.I), "placeholder video link"),
]
REQUIRED_PHRASES = {
    "docs/dependency-policy.md": [
        "metadata.json keeps dependencies as []",
        "full-stack co-load is verified",
        "optional runtime clients",
    ],
    "docs/upstream/delivery-receive-poll-api.md": [
        "messageReceived events exist",
        "no synchronous receive/poll query API",
        "agent.receive remains persisted-inbox polling",
    ],
    "docs/acceptance-gap-audit.md": [
        "Acceptance gap audit",
        "Closed",
        "Residual",
    ],
    "docs/upstream/program-live-api.md": [
        "No in-process LEZ program SDK or C ABI",
        "program.call fails closed",
        "rc3 SPEL path exists",
    ],
}

def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    raise SystemExit(1)

def main() -> int:
    meta = json.loads((ROOT/"metadata.json").read_text())
    if meta.get("dependencies") != []:
        fail("metadata.json dependencies must stay [] for standalone/progressive loading")

    for rel in REQUIRED_FILES:
        if not (ROOT/rel).exists():
            fail(f"missing required readiness artifact: {rel}")

    for rel, phrases in REQUIRED_PHRASES.items():
        text = (ROOT/rel).read_text(errors="ignore")
        for phrase in phrases:
            if phrase not in text:
                fail(f"{rel} missing required phrase: {phrase}")

    for rel in PUBLIC_FILES:
        text = (ROOT/rel).read_text(errors="ignore")
        for rx, desc in FORBIDDEN_PATTERNS:
            m = rx.search(text)
            if m:
                line = text[:m.start()].count("\n") + 1
                fail(f"{rel}:{line}: {desc}: {m.group(0)!r}")

    deep = (ROOT/"scripts/run_lp0008_deep_verify.py").read_text(errors="ignore")
    for needle in ["agent.receive", "storage.download", "wallet.send", "23"]:
        if needle not in deep:
            fail(f"deep verify missing {needle}")

    impl = (ROOT/"src/agent_module_impl.cpp").read_text(errors="ignore")
    for forbidden in ["r[\"submitted\"] = true;", "r[\"deployed\"] = true;"]:
        if forbidden in impl:
            fail(f"program.* must not fake live success: {forbidden}")

    integration = (ROOT/"scripts/run_logoscore_integration.sh").read_text(errors="ignore")
    for needle in ["storage_module", "delivery_module", "messaging.send"]:
        if needle not in integration:
            fail(f"integration harness missing {needle}")

    print("acceptance_readiness_ok")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
