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
PUBLIC_FILES = ["README.md", "SUBMISSION.md", "VIDEO_SCRIPT.md"]
REQUIRED_FILES = [
    "docs/dependency-policy.md",
    "docs/upstream/delivery-receive-poll-api.md",
    "docs/acceptance-gap-audit.md",
    "docs/upstream/program-live-api.md",
    "docs/final-video-audio-narration.md",
    "docs/strict-success-criteria-evidence.md",
    "docs/submission-readiness-matrix.md",
    "docs/cu-costs.md",
    "docs/three-use-cases.md",
    "docs/resilience-evidence.md",
    "scripts/run_three_use_cases_demo.sh",
    "scripts/run_resilience_evidence.sh",
    "scaffold.toml",
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
    "docs/strict-success-criteria-evidence.md": [
        "scripts/run_final_pre_video_evidence.sh",
        "Three separate agents",
        "**not** a final acceptance claim",
        "5dcf1b318ff5aadf5a8bff9843de71184b0f1c16e6234163373315a144df1fd3",
    ],
    "docs/submission-readiness-matrix.md": [
        "not ready for final Lambda Prize submission yet",
        "Basecamp owner-channel",
        "transport.result.mode == live",
        "CU cost documented",
    ],
    "docs/cu-costs.md": [
        "CU cost and proof-mode notes",
        "CU field not exposed",
        "RISC0_DEV_MODE=0 boundary",
    ],
    "docs/three-use-cases.md": [
        "personal file vault",
        "agent services marketplace",
        "multi-agent workflow",
        "Strict boundary",
    ],
    "scripts/run_three_use_cases_demo.sh": [
        "ASSERT three illustrative use cases",
        "payment_submitted",
        "summary.json",
    ],
    "docs/resilience-evidence.md": [
        "pending approval persistence",
        "timeout/fail-closed",
        "Skill failure isolation",
        "Strict boundary",
    ],
    "scripts/run_resilience_evidence.sh": [
        "ASSERT resilience",
        "approval_expired",
        "unknown_skill",
        "summary.json",
    ],
    "scaffold.toml": [
        "[modules.agent_module]",
        "path:.#lgx",
        "role = \"project\"",
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

    final_gate = ROOT/"scripts/run_final_pre_video_evidence.sh"
    if not final_gate.exists():
        fail("missing final pre-video evidence gate")
    gate_text = final_gate.read_text(errors="ignore")
    for needle in ["run_logoscore_integration.sh all", "run_lp0008_deep_verify.py", "run_multi_agent_a2a_demo.sh", "run_three_use_cases_demo.sh", "run_resilience_evidence.sh", "run_live_wallet_send_verify.py", "PRE_VIDEO_EVIDENCE_OK"]:
        if needle not in gate_text:
            fail(f"final evidence gate missing {needle}")

    submission = (ROOT/"SUBMISSION.md").read_text(errors="ignore")
    for phrase in ["not final-submission-ready yet", "Basecamp artifact readiness only", "Autonomous inter-agent LEZ payment tied to task acceptance is not yet proven"]:
        if phrase not in submission:
            fail(f"SUBMISSION.md missing honest no-go phrase: {phrase}")

    print("submission_honesty_ok")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
