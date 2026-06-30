#!/usr/bin/env python3
"""Verify LP-0008 default-branch GitHub Actions is green for the exact public SHA.

This is intentionally network/read-only and stdlib-first. It prefers gh when
available because authenticated API quotas are less brittle, then falls back to
GitHub REST without auth for public repos.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPO = "Tranquil-Flow/lp-0008-autonomous-agent"
DEFAULT_BRANCH = "main"


def run(cmd: list[str]) -> str:
    return subprocess.check_output(cmd, cwd=ROOT, text=True).strip()


def current_sha() -> str:
    return run(["git", "rev-parse", "HEAD"])


def origin_branch_sha(branch: str) -> str:
    subprocess.check_call(["git", "fetch", "--quiet", "origin", branch], cwd=ROOT)
    return run(["git", "rev-parse", f"origin/{branch}"])


def gh_runs(repo: str, branch: str, limit: int) -> list[dict]:
    out = subprocess.check_output(
        [
            "gh", "run", "list",
            "--repo", repo,
            "--branch", branch,
            "--limit", str(limit),
            "--json", "databaseId,status,conclusion,headSha,name,url,createdAt",
        ],
        text=True,
    )
    return json.loads(out)


def rest_runs(repo: str, branch: str, limit: int) -> list[dict]:
    # The public Actions API branch query can be cache/bot brittle; fetch recent
    # runs and filter head_branch locally so the fallback behaves like gh.
    url = f"https://api.github.com/repos/{repo}/actions/runs?per_page={limit}"
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "lp0008-ci-check"}
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = json.loads(resp.read().decode())
    runs = []
    for r in data.get("workflow_runs", []):
        if r.get("head_branch") != branch:
            continue
        runs.append({
            "databaseId": r.get("id"),
            "status": r.get("status"),
            "conclusion": r.get("conclusion"),
            "headSha": r.get("head_sha"),
            "name": r.get("name"),
            "url": r.get("html_url"),
            "createdAt": r.get("created_at"),
        })
    return runs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=DEFAULT_REPO)
    ap.add_argument("--branch", default=DEFAULT_BRANCH)
    ap.add_argument("--sha", default=None, help="SHA to require; defaults to git HEAD")
    ap.add_argument("--limit", type=int, default=20)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    sha = args.sha or current_sha()
    public_sha = origin_branch_sha(args.branch)
    if public_sha != sha:
        print(
            f"LP0008_CI_FAIL local/default SHA mismatch: requested={sha} origin/{args.branch}={public_sha}",
            file=sys.stderr,
        )
        return 1

    try:
        runs = gh_runs(args.repo, args.branch, args.limit)
        source = "gh"
    except Exception as exc:
        gh_error = str(exc)
        try:
            runs = rest_runs(args.repo, args.branch, args.limit)
            source = "rest"
        except Exception as rest_exc:
            print(f"LP0008_CI_FAIL could not query GitHub Actions via gh ({gh_error}) or REST ({rest_exc})", file=sys.stderr)
            return 1

    matches = [r for r in runs if r.get("headSha") == sha]
    if not matches:
        print(f"LP0008_CI_FAIL no Actions run found for {sha} on {args.repo}:{args.branch}", file=sys.stderr)
        return 1
    run_info = matches[0]
    if run_info.get("status") != "completed" or run_info.get("conclusion") != "success":
        print(
            "LP0008_CI_FAIL run not green: "
            + json.dumps(run_info, sort_keys=True),
            file=sys.stderr,
        )
        return 1

    out = {
        "ok": True,
        "repo": args.repo,
        "branch": args.branch,
        "sha": sha,
        "query_source": source,
        "run": run_info,
    }
    if args.json:
        print(json.dumps(out, indent=2, sort_keys=True))
    else:
        print(
            "LP0008_CI_OK sha={sha} run={run} url={url}".format(
                sha=sha,
                run=run_info.get("databaseId"),
                url=run_info.get("url"),
            )
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
