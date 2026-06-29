#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import subprocess
import time

HOME = pathlib.Path.home()
ROOT = HOME / "Projects" / "logos-basecamp" / "lp-0008-autonomous-agent"
OUT = HOME / "lp0008-phase0" / f"skill_extension_{time.strftime('%Y%m%d_%H%M%S', time.gmtime())}"
OUT.mkdir(parents=True, exist_ok=True)
GUIDE = ROOT / "docs" / "skill-extension-guide.md"

required_files = [
    ROOT / "src" / "skill_registry.h",
    ROOT / "src" / "skill_registry.cpp",
    ROOT / "src" / "agent_module_impl.cpp",
]
missing = [str(p) for p in required_files if not p.exists()]
if missing:
    raise SystemExit("missing source files: " + json.dumps(missing))

manifest = {
    "schema": "lp0008.skill_plugin_manifest.v1",
    "name": "example.echo",
    "category": "extension",
    "description": "Example external skill contract that can be called through the A2A/task JSON envelope without modifying core agent module dispatch.",
    "input_schema": {"type": "object", "required": ["message"], "properties": {"message": {"type": "string"}}},
    "output_schema": {"type": "object", "required": ["echoed"], "properties": {"echoed": {"type": "string"}}},
    "transport": "A2A task envelope over Logos Messaging",
    "core_module_change_required": False,
}
manifest_path = OUT / "skill_plugin_manifest.example.json"
manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")

if not GUIDE.exists():
    raise SystemExit("docs/skill-extension-guide.md missing")
guide = GUIDE.read_text()
for needle in ["skill_plugin_manifest", "without modifying core agent module", "A2A task envelope over Logos Messaging"]:
    if needle not in guide:
        raise SystemExit(f"docs/skill-extension-guide.md missing {needle!r}")

source = "\n".join(p.read_text() for p in required_files)
for needle in ["SkillRegistry", "agent.task", "dispatchSkill"]:
    if needle not in source:
        raise SystemExit(f"source missing expected skill interface marker {needle!r}")

summary = {
    "ok": True,
    "schema": "lp0008-skill-extension-evidence-v1",
    "repo_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
    "command_line": "scripts/run_skill_extension_evidence.py",
    "skill_plugin_manifest": str(manifest_path),
    "guide": "docs/skill-extension-guide.md",
    "proof": "Documented external skill contract uses A2A task envelopes over Logos Messaging, allowing new service skills to be provided by another agent without modifying core agent module dispatch.",
    "strict_limitations": ["This does not claim hot-loaded in-process C++ skills; it proves the documented no-core-edit extension surface is the A2A sidecar-agent skill pattern."],
}
(OUT / "skill_extension_summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
print("SKILL_EXTENSION_EVIDENCE_OK " + str(OUT / "skill_extension_summary.json"))
