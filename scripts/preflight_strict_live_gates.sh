#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${LP0008_EVIDENCE_DIR:-$HOME/lp0008-phase0}"
TS="$(date -u +%Y%m%d_%H%M%S)"
LOG="$OUT_DIR/strict_live_gate_preflight_$TS.log"
JSON="$OUT_DIR/strict_live_gate_preflight_$TS.json"
DOC="$ROOT/docs/strict-live-gate-inventory.md"
mkdir -p "$OUT_DIR" "$ROOT/docs"

pass=0; warn=0; fail=0
rows=()
json_items=()

json_escape() { python3 -c 'import json,sys; print(json.dumps(sys.stdin.read().rstrip("\n")))'; }
add_item() {
  local status="$1" name="$2" detail="$3" required="$4"
  rows+=("| $status | $required | $name | ${detail//|//} |")
  local jdetail; jdetail=$(printf '%s' "$detail" | json_escape)
  json_items+=("{\"status\":\"$status\",\"required\":\"$required\",\"name\":\"$name\",\"detail\":$jdetail}")
  case "$status" in
    PASS) pass=$((pass+1));;
    WARN) warn=$((warn+1));;
    FAIL) fail=$((fail+1));;
  esac
}
check_cmd() {
  local name="$1" cmd="$2" required="$3"
  if command -v "$cmd" >/dev/null 2>&1; then add_item PASS "$name" "$(command -v "$cmd")" "$required"; else add_item FAIL "$name" "missing command: $cmd" "$required"; fi
}
check_path() {
  local name="$1" path="$2" required="$3" kind="${4:-exists}"
  if [ "$kind" = "exec" ]; then
    if [ -x "$path" ]; then add_item PASS "$name" "$path" "$required"; else add_item FAIL "$name" "not executable: $path" "$required"; fi
  elif [ "$kind" = "dir" ]; then
    if [ -d "$path" ]; then add_item PASS "$name" "$path" "$required"; else add_item FAIL "$name" "missing directory: $path" "$required"; fi
  else
    if [ -e "$path" ]; then add_item PASS "$name" "$path" "$required"; else add_item FAIL "$name" "missing path: $path" "$required"; fi
  fi
}

exec > >(tee "$LOG") 2>&1

echo "LP-0008 strict live gate preflight"
echo "timestamp=$TS"
echo "root=$ROOT"
cd "$ROOT"
echo "repo_sha=$(git rev-parse HEAD)"
echo

check_cmd git git required
check_cmd python3 python3 required
check_cmd nix nix required
check_cmd lgs lgs required
check_cmd curl curl recommended
check_cmd nc nc recommended

LOGOSCORE="$HOME/lp0008-phase0/logoscore-cli/bin/logoscore"
  WALLET="$HOME/Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet"
  WALLET_HOME="${LP0008_LIVE_WALLET_HOME:-$HOME/lp0008-phase0/rc3_faucet_wallet}"
  check_path logoscore "$LOGOSCORE" required exec
  check_path rc3-wallet-binary "$WALLET" required exec
  check_path funded-wallet-home "$WALLET_HOME" required dir
  check_path basecamp-source-cache "$HOME/Library/Caches/logos-scaffold/repos/basecamp" required dir
  check_path basecamp-module-cache "$HOME/Library/Caches/logos-scaffold/basecamp" required dir
  check_path scaffold.toml "$ROOT/scaffold.toml" required exists

  if [ -x "$WALLET" ] && [ -d "$WALLET_HOME" ]; then
    set +e
    acct_out=$(NSSA_WALLET_HOME_DIR="$WALLET_HOME" "$WALLET" account list 2>&1)
    acct_rc=$?
    set -e
    if [ "$acct_rc" -eq 0 ]; then
      pub_count=$(printf '%s\n' "$acct_out" | grep -c 'Public/' || true)
      priv_count=$(printf '%s\n' "$acct_out" | grep -c 'Private/' || true)
      # Do not print full account IDs in preflight; exact IDs belong in strict evidence after funding checks.
      if [ "$priv_count" -ge 3 ]; then
        add_item PASS wallet-private-account-count "private_accounts=$priv_count public_accounts=$pub_count" required
      else
        add_item FAIL wallet-private-account-count "private_accounts=$priv_count public_accounts=$pub_count; need >=3 for strict three-agent proof" required
      fi
    else
      add_item FAIL wallet-account-list "wallet account list failed rc=$acct_rc: $(printf '%s' "$acct_out" | head -c 240)" required
    fi
  fi

  if [ -x "$WALLET" ] && [ -d "$WALLET_HOME" ]; then
    set +e
    health_out=$(NSSA_WALLET_HOME_DIR="$WALLET_HOME" "$WALLET" check-health 2>&1)
    health_rc=$?
    set -e
    if [ "$health_rc" -eq 0 ]; then
      add_item PASS wallet-check-health "wallet connected to configured node" required
    else
      add_item WARN wallet-check-health "rc=$health_rc; $(printf '%s' "$health_out" | head -c 300)" required
    fi
  fi

  if python3 - <<'PY' >/tmp/lp0008_seq_probe.out 2>&1
import urllib.request, urllib.error
url='https://testnet.lez.logos.co/'
try:
    with urllib.request.urlopen(url, timeout=10) as r:
        print(r.status)
except urllib.error.HTTPError as e:
    # HTTP 405 still proves the sequencer endpoint is reachable over TLS.
    print(e.code)
except Exception as e:
    raise SystemExit(str(e))
PY
  then
    add_item PASS lez-sequencer-reachability "https://testnet.lez.logos.co reachable" required
  else
    add_item WARN lez-sequencer-reachability "probe failed: $(head -c 240 /tmp/lp0008_seq_probe.out)" required
  fi

  if /usr/bin/nc -z localhost 5900 >/dev/null 2>&1; then
    add_item PASS screen-sharing-vnc "localhost:5900 accepts connections" recommended
  else
    add_item WARN screen-sharing-vnc "not listening on localhost:5900; needed only for remote GUI recording" recommended
  fi

  if [ -f "$HOME/.config/gh/hosts.yml" ]; then
    if grep -q "oauth_token:" "$HOME/.config/gh/hosts.yml"; then add_item PASS github-token "token present in gh hosts.yml (not printed)" required
    else add_item WARN github-token "hosts.yml exists but oauth_token not found" required
    fi
  else
    add_item WARN github-token "missing ~/.config/gh/hosts.yml on remote; push may require token scp from container" required
  fi

  printf '\n| Status | Required | Gate | Detail |\n|---|---|---|---|\n'
  printf '%s\n' "${rows[@]}"

{
  printf '{\n'
  printf '  "schema": "lp0008-strict-live-gate-preflight-v1",\n'
  printf '  "timestamp": "%s",\n' "$TS"
  printf '  "repo_sha": "%s",\n' "$(cd "$ROOT" && git rev-parse HEAD)"
  printf '  "log": "%s",\n' "$LOG"
  printf '  "counts": {"PASS": %d, "WARN": %d, "FAIL": %d},\n' "$pass" "$warn" "$fail"
  printf '  "items": [\n'
  for i in "${!json_items[@]}"; do
    sep=","; [ "$i" -eq "$((${#json_items[@]}-1))" ] && sep=""
    printf '    %s%s\n' "${json_items[$i]}" "$sep"
  done
  printf '  ]\n}\n'
} > "$JSON"

{
  echo '# LP-0008 strict live gate inventory'
  echo
  echo "Generated: $TS UTC"
  echo "Repo SHA: \`$(cd "$ROOT" && git rev-parse HEAD)\`"
  echo "Raw log: \`$LOG\`"
  echo "JSON: \`$JSON\`"
  echo
  echo "Counts: PASS=$pass WARN=$warn FAIL=$fail"
  echo
  echo '| Status | Required | Gate | Detail |'
  echo '|---|---|---|---|'
  printf '%s\n' "${rows[@]}"
  echo
  echo 'Warnings do not close strict criteria. They identify live gates that must be solved, manually supplied, or explicitly bounded before final video.'
} > "$DOC"

python3 "$ROOT/scripts/write_strict_evidence_manifest.py" add \
  --criterion PHASE1_PREFLIGHT \
  --status "$([ "$fail" -eq 0 ] && echo pass || echo fail)" \
  --script scripts/preflight_strict_live_gates.sh \
  --artifact "$LOG" \
  --artifact "$JSON" \
  --artifact docs/strict-live-gate-inventory.md \
  --note "strict live gate preflight PASS=$pass WARN=$warn FAIL=$fail" >/dev/null

echo "PREFLIGHT_LOG=$LOG"
echo "PREFLIGHT_JSON=$JSON"
echo "PREFLIGHT_DOC=$DOC"
echo "PREFLIGHT_COUNTS=PASS:$pass WARN:$warn FAIL:$fail"
[ "$fail" -eq 0 ]
