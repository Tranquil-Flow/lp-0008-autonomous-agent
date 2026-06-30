#!/usr/bin/env python3
"""Guard LP-0008 A2A schema hardening evidence."""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TRACE = ROOT / "docs" / "strict-traceability-matrix.md"
READY = ROOT / "docs" / "submission-readiness-matrix.md"
VALIDATOR = ROOT / "scripts" / "validate_acceptance_readiness.py"
GATE = ROOT / "scripts" / "run_final_strict_evidence.sh"
SCRIPT = ROOT / "scripts" / "run_a2a_schema_evidence.py"
DOC = ROOT / "docs" / "a2a-schema-evidence.md"


def fail(msg: str) -> None:
    print(f"A2A_SCHEMA_HARDENING_FAIL {msg}", file=sys.stderr)
    raise SystemExit(1)


def row_status(row_id: str) -> str:
    for line in TRACE.read_text().splitlines():
        if line.startswith(f"| {row_id} |"):
            cols = [c.strip() for c in line.strip().strip("|").split("|")]
            return cols[6]
    fail(f"missing trace row {row_id}")


def main() -> None:
    if not SCRIPT.exists():
        fail("missing scripts/run_a2a_schema_evidence.py")
    if not DOC.exists():
        fail("missing docs/a2a-schema-evidence.md")
    if row_status("F7") != "Met":
        fail(f"F7 status is {row_status('F7')!r}, expected 'Met'")
    gate = GATE.read_text()
    if "a2a-schema" not in gate or "run_a2a_schema_evidence.py" not in gate:
        fail("final strict gate does not run A2A schema evidence")
    validator = VALIDATOR.read_text()
    for needle in ["run_a2a_schema_evidence.py", "docs/a2a-schema-evidence.md", "A2A_SCHEMA_EVIDENCE_OK"]:
        if needle not in validator:
            fail(f"acceptance validator missing {needle}")
    ready = READY.read_text()
    for needle in ["A2A schema validation", "Agent Card", "task lifecycle envelope"]:
        if needle not in ready:
            fail(f"readiness matrix missing {needle}")
    script = SCRIPT.read_text()
    for needle in ["protocolVersion", "a2a.task.request", "queued", "transport_sent", "completed"]:
        if needle not in script:
            fail(f"schema evidence script missing {needle}")
    print("A2A_SCHEMA_HARDENING_OK")


if __name__ == "__main__":
    main()
