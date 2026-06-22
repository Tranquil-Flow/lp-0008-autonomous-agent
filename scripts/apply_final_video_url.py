#!/usr/bin/env python3
"""Apply final LP-0008 video URL to implementation and optional lambda-prize draft.

Usage:
  scripts/apply_final_video_url.py https://example.com/video
  LAMBDA_PRIZE_WORKTREE=$HOME/lp0008-phase0/lambda-prize-local scripts/apply_final_video_url.py https://example.com/video
"""
from pathlib import Path
import os, re, sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_final_video_url.py <https-video-url>")
url = sys.argv[1].strip()
if not re.match(r"^https://", url):
    raise SystemExit("video URL must be https://")
if any(x in url.lower() for x in ["pending", "placeholder", "example.com"]):
    raise SystemExit("video URL still looks like a placeholder")

repo = Path(__file__).resolve().parents[1]
paths = [repo/"SUBMISSION.md"]
lambda_root = os.environ.get("LAMBDA_PRIZE_WORKTREE")
if lambda_root:
    paths.append(Path(lambda_root)/"solutions/LP-0008.md")

for path in paths:
    if not path.exists():
        raise SystemExit(f"missing file: {path}")
    text = path.read_text()
    if "PENDING_VIDEO_URL" not in text:
        raise SystemExit(f"PENDING_VIDEO_URL not found in {path}")
    path.write_text(text.replace("PENDING_VIDEO_URL", url))
    print(f"updated {path}")
