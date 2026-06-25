#!/usr/bin/env python3
"""Audit LP-0008 strict traceability docs.

Fails on missing upstream source, unmapped criteria, TBD placeholders, malformed table rows,
or final-video unsafe status drift. This is intentionally stdlib-only so it can run in CI and fresh clones.
"""
from __future__ import annotations
import argparse, json, re, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/prize-criteria-source.md"
MATRIX = ROOT / "docs/strict-traceability-matrix.md"
ALLOWED = {"Planned", "Met", "Blocked by upstream API", "Blocked by missing credential", "Accepted residual risk"}
FINAL_BLOCKING = {"Planned", "Blocked by missing credential"}

def fail(msg: str) -> None:
    raise SystemExit(f"TRACEABILITY_AUDIT_FAIL: {msg}")

def fetch_upstream() -> str:
    import tempfile
    with tempfile.TemporaryDirectory(prefix="lp0008-prize-") as d:
        repo = Path(d) / "lambda-prize"
        subprocess.check_call(["git", "clone", "--quiet", "--depth", "1", "https://github.com/logos-co/lambda-prize.git", str(repo)])
        sha = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
        text = (repo / "prizes/LP-0008.md").read_text()
        return sha + "\n" + text

def parse_matrix() -> list[dict[str, str]]:
    if not MATRIX.exists():
        fail(f"missing {MATRIX}")
    rows=[]
    for line in MATRIX.read_text().splitlines():
        if not line.startswith("| ") or line.startswith("|---") or line.startswith("| ID "):
            continue
        parts=[p.strip() for p in line.strip().strip('|').split('|')]
        if len(parts) != 9:
            fail(f"malformed table row with {len(parts)} columns: {line[:120]}")
        rows.append(dict(zip(["id","area","phrase","interpretation","scripts","artifact","status","notes","scene"], parts)))
    return rows

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--refresh-source', action='store_true', help='check current upstream LP-0008 text against snapshot')
    ap.add_argument('--final', action='store_true', help='fail if any row is still not final-ready')
    ap.add_argument('--json', action='store_true')
    args=ap.parse_args()
    if not SOURCE.exists():
        fail(f"missing {SOURCE}")
    src=SOURCE.read_text()
    if 'Upstream commit:' not in src or 'Full upstream LP-0008.md snapshot' not in src:
        fail('source snapshot missing upstream commit or full markdown snapshot')
    if 'TBD' in src:
        fail('source snapshot contains TBD')
    rows=parse_matrix()
    if len(rows) < 25:
        fail(f'too few mapped rows: {len(rows)}')
    ids=[r['id'] for r in rows]
    if len(ids) != len(set(ids)):
        fail('duplicate IDs in traceability matrix')
    required_prefixes=['F1','F8','F10','U2','R1','P1','S5','S6','SR1','EV1']
    missing=[x for x in required_prefixes if x not in ids]
    if missing:
        fail('missing required critical IDs: '+', '.join(missing))
    for r in rows:
        for k,v in r.items():
            if not v or v.upper() == 'TBD':
                fail(f"{r['id']} has empty/TBD {k}")
        if r['status'] not in ALLOWED:
            fail(f"{r['id']} invalid status {r['status']!r}")
        if not r['scripts'].startswith('scripts/') and '.github/' not in r['scripts']:
            fail(f"{r['id']} script mapping does not reference repo script/workflow: {r['scripts']}")
        if 'evidence/strict-latest/manifest.json' not in r['artifact']:
            fail(f"{r['id']} artifact does not point at strict manifest")
    if args.refresh_source:
        current=fetch_upstream()
        sha=current.split('\n',1)[0]
        if f'`{sha}`' not in src:
            fail(f'upstream LP-0008 source changed or snapshot stale: current {sha}')
    if args.final:
        blockers=[r['id'] for r in rows if r['status'] in FINAL_BLOCKING]
        if blockers:
            fail('not final-ready rows: '+', '.join(blockers))
    out={'ok': True, 'rows': len(rows), 'statuses': {}, 'checked_at': datetime.now(timezone.utc).isoformat()}
    for r in rows:
        out['statuses'][r['status']] = out['statuses'].get(r['status'], 0) + 1
    if args.json:
        print(json.dumps(out, indent=2, sort_keys=True))
    else:
        print('TRACEABILITY_AUDIT_OK rows=%d statuses=%s' % (out['rows'], out['statuses']))
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
