#!/usr/bin/env python3
"""Guard LP-0008 readiness docs against stale pre-hardening claims."""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TRACE = ROOT / "docs" / "strict-traceability-matrix.md"
READY = ROOT / "docs" / "submission-readiness-matrix.md"
SUBMISSION = ROOT / "SUBMISSION.md"

CURRENT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
STALE_SHAS = {
    "78342acfa82ffcaf74fb4b686500b18e6dd9f411",
    "bc5bdfad0ee901aea06a17564fe4c1028bac1b9f",
    "f23bb502bab704fe957fbc54eb7bd2cf93e9bd9e",
}

EXPECTED_NOT_PLANNED = {
    "F1": "Met",
    "F3": "Met",
    "F4": "Met",
    "F5": "Met",
    "F9": "Met",
    "F11": "Met",
    "U1": "Met",
    "U2": "Met",
    "R1": "Met",
    "R2": "Met",
    "S3": "Met",
    "S4": "Met",
    "SR1": "Met",
    "SR2": "Met",
    "SR5": "Met",
    "EV1": "Met",
}

EXPECTED_BLOCKED = {
    "P1": "Blocked by upstream API",
    "S5": "Blocked by upstream API",
}

EXPECTED_ACCEPTED_RISK = {
    "F2": "Accepted residual risk",
    "S2": "Accepted residual risk",
}

ALLOWED_PLANNED = {"S6", "SR3"}


def fail(msg: str) -> None:
    print(f"TRACEABILITY_FRESHNESS_FAIL {msg}", file=sys.stderr)
    raise SystemExit(1)


def row_status(text: str, row_id: str) -> str:
    for line in text.splitlines():
        if line.startswith(f"| {row_id} |"):
            cols = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cols) < 7:
                fail(f"malformed row {row_id}: {line}")
            return cols[6]
    fail(f"missing row {row_id}")


def main() -> None:
    trace = TRACE.read_text()
    ready = READY.read_text()
    submission = SUBMISSION.read_text()

    for stale in STALE_SHAS:
        if stale in trace or stale in ready or stale in submission:
            fail(f"stale sha still present: {stale}")

    for row, expected in EXPECTED_NOT_PLANNED.items():
        got = row_status(trace, row)
        if got != expected:
            fail(f"{row} status {got!r}, expected {expected!r}")

    for row, expected in EXPECTED_BLOCKED.items():
        got = row_status(trace, row)
        if got != expected:
            fail(f"{row} status {got!r}, expected {expected!r}")

    for row, expected in EXPECTED_ACCEPTED_RISK.items():
        got = row_status(trace, row)
        if got != expected:
            fail(f"{row} status {got!r}, expected {expected!r}")

    planned = []
    for line in trace.splitlines():
        if not line.startswith("| ") or line.startswith("| ID ") or line.startswith("|---"):
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) >= 7 and cols[6] == "Planned":
            planned.append(cols[0])
    unexpected = [row for row in planned if row not in ALLOWED_PLANNED]
    if unexpected:
        fail("unexpected non-video Planned rows: " + ", ".join(unexpected))

    if "headless Logos Core deployment" not in ready:
        fail("readiness matrix missing headless deployment hardening")
    if "sidecar skill-extension" not in ready:
        fail("readiness matrix missing sidecar skill-extension hardening")
    if "fresh-clone reproduction" not in ready:
        fail("readiness matrix missing fresh-clone hardening")

    print("TRACEABILITY_FRESHNESS_OK rows=%d planned=%s" % (len(EXPECTED_NOT_PLANNED) + len(EXPECTED_BLOCKED) + len(EXPECTED_ACCEPTED_RISK), ",".join(planned)))


if __name__ == "__main__":
    main()
