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
    "scripts/run_final_strict_evidence.sh",
    "scripts/run_a2a_schema_evidence.py",
    "docs/a2a-schema-evidence.md",
    "docs/a2a-schema-summary-latest.json",
    "scripts/check_github_ci.py",
    "docs/ci-evidence.md",
    "docs/final-strict-evidence-gate.md",
    "scripts/record_final_video.sh",
    "docs/final-submission-preflight.md",
    "scripts/apply_final_video_url.py",
    "scripts/finalize_after_video.sh",
    "scripts/check_lp0008_submission_window.py",
    "scaffold.toml",
    "module.json",
    "tests/validate_traceability_freshness.py",
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
        "dcb41d4c4a579541b591194f5701eed78762e8934a11f5a48f6fff607a974c73",
    ],
    "docs/submission-readiness-matrix.md": [
        "non-video evidence is closed or explicitly upstream/tooling-blocked",
        "Basecamp owner-channel",
        "transport.result.mode == live",
        "CU cost documented",
        "headless Logos Core deployment",
        "sidecar skill-extension",
        "fresh-clone reproduction",
        "scripts/check_github_ci.py",
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
    "docs/a2a-schema-evidence.md": [
        "A2A schema evidence",
        "a2a.task.request",
        "queued -> transport_sent -> working -> completed",
        "Strict boundary",
    ],
    "scripts/run_a2a_schema_evidence.py": [
        "A2A_SCHEMA_EVIDENCE_OK",
        "protocolVersion",
        "a2a.task.request",
        "queued",
        "transport_sent",
        "completed",
    ],
    "scripts/check_github_ci.py": [
        "LP0008_CI_OK",
        "origin_branch_sha",
        "headSha",
        "conclusion",
    ],
    "docs/ci-evidence.md": [
        "LP-0008 CI evidence",
        "origin/main == git rev-parse HEAD",
        "completed",
        "success",
        "Reviewer-facing boundary",
    ],
    "scripts/finalize_after_video.sh": [
        "FINALIZE_OK",
        "ask explicit approval",
        "validate-submission.sh",
        "check_lp0008_submission_window.py",
    ],
    "scripts/check_lp0008_submission_window.py": [
        "LP0008_SUBMISSION_WINDOW_OK",
        "OPEN_LP0008_SUBMISSION_EXISTS",
        "LP0008_COOLDOWN_NOT_READY",
    ],
    "docs/final-submission-preflight.md": [
        "non-video repository hardening complete or explicitly upstream/tooling-blocked",
        "PENDING_VIDEO_URL",
        "explicit approval",
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
    "docs/final-strict-evidence-gate.md": [
        "Final strict evidence gate",
        "run_final_strict_evidence.sh",
        "ok_with_blockers",
        "live-strict-evidence.yml",
    ],
    "scripts/run_final_strict_evidence.sh": [
        "FINAL_STRICT_EVIDENCE_COMPLETE",
        "run_step_allow_blocker",
        "paid-a2a-live",
        "a2a-schema",
        "strict-skill-matrix",
    ],
    "scripts/record_final_video.sh": [
        "LP-0008 Autonomous AI Agent",
        "run_lp0008_deep_verify.py",
        "run_three_use_cases_demo.sh",
        "run_resilience_evidence.sh",
    ],
    "scaffold.toml": [
        "[modules.agent_module]",
        "path:.#lgx",
        "role = \"project\"",
    ],
    "module.json": [
        "logos.messaging",
        "a2a.task-lifecycle",
        "nix build .#lgx",
    ],
    "tests/validate_traceability_freshness.py": [
        "TRACEABILITY_FRESHNESS_OK",
        "F3",
        "EV1",
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
    strict_gate = ROOT/"scripts/run_final_strict_evidence.sh"
    if not final_gate.exists():
        fail("missing final pre-video evidence gate")
    if not strict_gate.exists():
        fail("missing final strict evidence gate")
    gate_text = final_gate.read_text(errors="ignore")
    for needle in ["run_logoscore_integration.sh all", "run_lp0008_deep_verify.py", "run_multi_agent_a2a_demo.sh", "run_three_use_cases_demo.sh", "run_resilience_evidence.sh", "run_live_wallet_send_verify.py", "PRE_VIDEO_EVIDENCE_OK"]:
        if needle not in gate_text:
            fail(f"final evidence gate missing {needle}")
    strict_text = strict_gate.read_text(errors="ignore")
    for needle in ["pre-video-bundle", "a2a-schema", "paid-a2a-live", "basecamp-owner-approval", "program-cu-boundary", "strict-skill-matrix", "FINAL_STRICT_EVIDENCE_COMPLETE"]:
        if needle not in strict_text:
            fail(f"strict evidence gate missing {needle}")

    submission = (ROOT/"SUBMISSION.md").read_text(errors="ignore")
    for phrase in ["not final-submission-ready yet", "Basecamp artifact readiness only", "Autonomous inter-agent LEZ payment tied to task acceptance is not yet proven"]:
        if phrase not in submission:
            fail(f"SUBMISSION.md missing honest no-go phrase: {phrase}")

    print("submission_honesty_ok")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
