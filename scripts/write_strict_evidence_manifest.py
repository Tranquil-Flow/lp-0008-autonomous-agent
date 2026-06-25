#!/usr/bin/env python3
"""Create/update LP-0008 strict evidence manifest.

Usage:
  scripts/write_strict_evidence_manifest.py init
  scripts/write_strict_evidence_manifest.py add --criterion F8 --status pass --script scripts/run_paid_a2a_live_evidence.py --artifact /path/log --task-id ... --message-id ... --tx-hash ...
  scripts/write_strict_evidence_manifest.py summary
"""
from __future__ import annotations
import argparse, json, os, subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'evidence/strict-latest/manifest.json'

def sha() -> str:
    try:
        return subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip()
    except Exception:
        return 'unknown'

def load(path: Path) -> dict:
    if path.exists():
        return json.loads(path.read_text())
    return {
        'schema': 'lp0008-strict-evidence-manifest-v1',
        'repo': 'Tranquil-Flow/lp-0008-autonomous-agent',
        'repo_sha': sha(),
        'created_at': datetime.now(timezone.utc).isoformat(),
        'updated_at': datetime.now(timezone.utc).isoformat(),
        'criteria': {},
        'artifacts': [],
    }

def save(path: Path, data: dict) -> None:
    data['updated_at'] = datetime.now(timezone.utc).isoformat()
    data['repo_sha'] = sha()
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')
    tmp.replace(path)

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['init','add','summary'])
    ap.add_argument('--manifest', default=str(DEFAULT))
    ap.add_argument('--criterion')
    ap.add_argument('--status', choices=['pass','fail','blocked','skipped'])
    ap.add_argument('--script')
    ap.add_argument('--artifact', action='append', default=[])
    ap.add_argument('--tx-hash', action='append', default=[])
    ap.add_argument('--task-id', action='append', default=[])
    ap.add_argument('--message-id', action='append', default=[])
    ap.add_argument('--wallet-account', action='append', default=[])
    ap.add_argument('--note', default='')
    args=ap.parse_args()
    path=Path(args.manifest)
    data=load(path)
    if args.cmd == 'init':
        save(path, data)
        print(path)
        return 0
    if args.cmd == 'add':
        if not args.criterion or not args.status or not args.script:
            raise SystemExit('add requires --criterion --status --script')
        entry={
            'criterion': args.criterion,
            'status': args.status,
            'script': args.script,
            'artifacts': args.artifact,
            'tx_hashes': args.tx_hash,
            'task_ids': args.task_id,
            'message_ids': args.message_id,
            'wallet_accounts': args.wallet_account,
            'note': args.note,
            'recorded_at': datetime.now(timezone.utc).isoformat(),
            'command': ' '.join(os.sys.argv),
            'repo_sha': sha(),
        }
        data['criteria'][args.criterion] = entry
        data['artifacts'].extend(args.artifact)
        save(path, data)
        print(json.dumps(entry, indent=2, sort_keys=True))
        return 0
    # summary
    counts={}
    for entry in data.get('criteria', {}).values():
        counts[entry['status']] = counts.get(entry['status'], 0) + 1
    print(json.dumps({'manifest': str(path), 'repo_sha': data.get('repo_sha'), 'criteria': len(data.get('criteria', {})), 'counts': counts}, indent=2, sort_keys=True))
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
