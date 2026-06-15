#!/usr/bin/env python3
import json
import sys

if len(sys.argv) < 4:
    print("usage: assert_result.py <raw-result-file> <json-path> <expected-json>", file=sys.stderr)
    sys.exit(2)

raw_path, path, expected_text = sys.argv[1:4]
raw = open(raw_path, "r", encoding="utf-8").read().strip()

# C ABI dispatch JSON-serializes C++ string returns, so parse outer JSON first.
try:
    value = json.loads(raw)
except json.JSONDecodeError:
    value = raw

# Most module methods return a JSON string. Parse that too when possible.
if isinstance(value, str):
    try:
        parsed = json.loads(value)
        value = parsed
    except json.JSONDecodeError:
        pass

try:
    expected = json.loads(expected_text)
except json.JSONDecodeError:
    expected = expected_text

cur = value
if path != ".":
    for part in path.lstrip(".").split("."):
        if part == "":
            continue
        if isinstance(cur, list):
            cur = cur[int(part)]
        elif isinstance(cur, dict):
            cur = cur[part]
        else:
            raise TypeError(f"cannot descend through {type(cur).__name__} at {part}")

if cur != expected:
    print(f"ASSERT FAIL {path}: expected {expected!r}, got {cur!r}", file=sys.stderr)
    print("Full decoded value:", json.dumps(value, indent=2, sort_keys=True), file=sys.stderr)
    sys.exit(1)

print(f"ok {path} == {expected!r}")
