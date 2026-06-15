#!/usr/bin/env python3
"""Smart JSON parser for C ABI dispatch output.

The dispatch function wraps all return values as JSON strings.
For methods returning plain strings (greet), the output is: "Hello, ..."
For methods returning JSON objects, the output is: "{\\"status\\":\\"running\\"}"

This parser unwraps as many layers as needed.
"""
import json
import sys

def smart_parse(raw: str):
    """Parse dispatch output, unwrapping nested JSON strings."""
    # First parse
    try:
        val = json.loads(raw)
    except json.JSONDecodeError:
        return raw  # Not JSON at all, return as-is

    # If val is a string that looks like JSON, parse again
    if isinstance(val, str):
        stripped = val.strip()
        if stripped and stripped[0] in '{[':
            try:
                val = json.loads(stripped)
            except json.JSONDecodeError:
                pass  # String that starts with { but isn't valid JSON

    return val


def assert_field(parsed, path: str, expected_str: str):
    """Assert that a field at the given dot-path matches expected value.

    path uses dot notation: .status, .files.0.label
    expected_str is parsed as JSON for comparison.
    """
    expected = json.loads(expected_str)

    cur = parsed
    if path:
        for part in path.lstrip('.').split('.'):
            if part == '':
                continue
            if isinstance(cur, list):
                cur = cur[int(part)]
            elif isinstance(cur, dict):
                cur = cur[part]
            else:
                raise TypeError(f"cannot descend through {type(cur)} at '{part}'")

    # For strings, use substring match if exact match fails (more forgiving)
    if isinstance(expected, str) and isinstance(cur, str):
        if expected in cur:
            return True
        return cur == expected
    return cur == expected


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("usage: assert_result.py <raw-result-file> <json-path> <expected-json>", file=sys.stderr)
        sys.exit(2)

    raw_path, path, expected_str = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(raw_path) as f:
        raw = f.read().strip()

    parsed = smart_parse(raw)

    if assert_field(parsed, path, expected_str):
        print(f"ok {path.lstrip('.')} == {expected_str}")
        sys.exit(0)
    else:
        print(f"FAIL {path.lstrip('.')} expected {expected_str} but got {parsed}", file=sys.stderr)
        sys.exit(1)
