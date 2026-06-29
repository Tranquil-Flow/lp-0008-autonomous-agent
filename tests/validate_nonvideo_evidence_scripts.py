#!/usr/bin/env python3
"""Static guardrails for final non-video LP-0008 evidence scripts.

These tests are deliberately cheap: they prove the final strict gate wires the
headless deploy, fresh-clone reproduction, and skill-extension evidence surfaces
before expensive live evidence is run.
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(path: str, needles: list[str]) -> str:
    p = ROOT / path
    assert p.exists(), f"missing required evidence script/doc: {path}"
    text = p.read_text()
    for needle in needles:
        assert needle in text, f"{path} missing required marker: {needle}"
    return text


def test_headless_deploy_evidence_script_documents_single_cli_flow():
    require("scripts/run_headless_deploy_evidence.py", [
        "HEADLESS_DEPLOY_EVIDENCE_OK",
        "single_cli_command",
        "load-module",
        "meta.configure",
        "LP0008_DISABLE_WALLET_FFI",
    ])


def test_skill_extension_evidence_script_proves_no_core_edit_extension_surface():
    require("scripts/run_skill_extension_evidence.py", [
        "SKILL_EXTENSION_EVIDENCE_OK",
        "skill_plugin_manifest",
        "without modifying core agent module",
        "docs/skill-extension-guide.md",
    ])


def test_fresh_clone_repro_script_runs_clone_build_demo_validator_path():
    require("scripts/run_fresh_clone_strict_repro.sh", [
        "FRESH_CLONE_STRICT_REPRO_OK",
        "git clone",
        "nix build .#install",
        "scripts/validate_acceptance_readiness.py",
    ])


def test_final_strict_gate_wires_new_nonvideo_evidence_scripts():
    gate = require("scripts/run_final_strict_evidence.sh", [
        "headless-deploy",
        "skill-extension",
        "fresh-clone-repro",
    ])
    assert gate.index("headless-deploy") < gate.index("acceptance-readiness")
    assert gate.index("skill-extension") < gate.index("acceptance-readiness")
    assert gate.index("fresh-clone-repro") < gate.index("acceptance-readiness")


if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for test in tests:
        test()
    print(f"NONVIDEO_EVIDENCE_SCRIPT_TESTS_OK tests={len(tests)}")
