#!/usr/bin/env python3
"""Check LP-0008 Lambda Prize submission field and same-team cooldown.

This script is intentionally read-only. It queries public GitHub PR metadata for
`logos-co/lambda-prize`, reports existing LP-0008 submissions, flags any open
same-prize PRs, and computes the 7-day+30-minute same-builder cooldown for the
configured builder login.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import sys
import urllib.parse
import urllib.request

REPO = "logos-co/lambda-prize"
LP_ID = "LP-0008"
DEFAULT_BUILDER = "Tranquil-Flow"
TOKEN = os.environ.get("GITHUB_TOKEN", "").strip()
KNOWN_LP0008_PRS = {34, 81, 85, 88}


def github_token_from_gh_hosts() -> str:
    hosts = Path.home() / ".config" / "gh" / "hosts.yml"
    if not hosts.exists():
        return ""
    try:
        import yaml  # type: ignore
        data = yaml.safe_load(hosts.read_text()) or {}
        return (data.get("github.com", {}) or {}).get("oauth_token") or (data.get("github.com", {}) or {}).get("token") or ""
    except Exception:
        # Keep script usable without PyYAML; public endpoints still work unauthenticated.
        return ""


def request_json(url: str) -> dict:
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "User-Agent": "lp0008-submission-window-check",
    }
    token = TOKEN or github_token_from_gh_hosts()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def search_issues(query: str) -> list[dict]:
    url = "https://api.github.com/search/issues?q=" + urllib.parse.quote(query) + "&per_page=100"
    return request_json(url).get("items", [])


def get_pull(number: int) -> dict:
    return request_json(f"https://api.github.com/repos/{REPO}/pulls/{number}")


def parse_dt(value: str | None) -> datetime | None:
    if not value:
        return None
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def main() -> int:
    builder = os.environ.get("LP0008_BUILDER_LOGIN", DEFAULT_BUILDER)
    hits: dict[int, dict] = {}
    for query in [
        f"repo:{REPO} is:pr {LP_ID}",
        f"repo:{REPO} is:pr \"solutions/{LP_ID}.md\"",
        f"repo:{REPO} is:pr \"Autonomous AI Agent\"",
    ]:
        for item in search_issues(query):
            hits[item["number"]] = item

    pulls = [get_pull(n) for n in sorted(hits)]
    relevant = [
        p for p in pulls
        if p["number"] in KNOWN_LP0008_PRS
        or LP_ID in (p.get("title") or "")
        or f"solutions/{LP_ID}.md" in (p.get("body") or "")
        or "Autonomous AI Agent" in (p.get("title") or "")
    ]

    open_prs = [p for p in relevant if p["state"] == "open"]
    builder_prs = [p for p in relevant if (p.get("user") or {}).get("login", "").lower() == builder.lower()]
    closed_builder = [p for p in builder_prs if p.get("closed_at")]
    closed_builder.sort(key=lambda p: parse_dt(p["closed_at"]) or datetime.min.replace(tzinfo=timezone.utc), reverse=True)

    now = datetime.now(timezone.utc)
    if closed_builder:
        last_closed = parse_dt(closed_builder[0]["closed_at"])
        assert last_closed is not None
        safe_at = last_closed + timedelta(days=7, minutes=30)
        cooldown = {
            "last_closed_pr": closed_builder[0]["number"],
            "last_closed_at": closed_builder[0]["closed_at"],
            "safe_at": safe_at.isoformat().replace("+00:00", "Z"),
            "open_now": now >= safe_at,
            "seconds_until_safe": max(0, int((safe_at - now).total_seconds())),
        }
    else:
        cooldown = {
            "last_closed_pr": None,
            "last_closed_at": None,
            "safe_at": None,
            "open_now": True,
            "seconds_until_safe": 0,
        }

    summary = {
        "schema": "lp0008-submission-window-v1",
        "repo": REPO,
        "lp_id": LP_ID,
        "builder_login": builder,
        "checked_at": now.isoformat().replace("+00:00", "Z"),
        "relevant_pr_count": len(relevant),
        "open_relevant_pr_count": len(open_prs),
        "same_builder_pr_count": len(builder_prs),
        "cooldown": cooldown,
        "prs": [
            {
                "number": p["number"],
                "state": p["state"],
                "title": p["title"],
                "user": p["user"]["login"],
                "head": p["head"]["label"],
                "created_at": p["created_at"],
                "closed_at": p["closed_at"],
                "merged_at": p["merged_at"],
                "url": p["html_url"],
            }
            for p in relevant
        ],
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    if open_prs:
        print(f"OPEN_LP0008_SUBMISSION_EXISTS count={len(open_prs)}", file=sys.stderr)
        return 1
    if not cooldown["open_now"]:
        print("LP0008_COOLDOWN_NOT_READY", file=sys.stderr)
        return 1
    print("LP0008_SUBMISSION_WINDOW_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
