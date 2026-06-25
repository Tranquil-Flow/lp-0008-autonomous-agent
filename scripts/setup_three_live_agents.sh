#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TS="$(date -u +%Y%m%d_%H%M%S)"
STATE_ROOT="${LP0008_THREE_AGENT_STATE:-$HOME/lp0008-phase0/three_live_agents_strict_latest}"
WALLET_HOME="${LP0008_LIVE_WALLET_HOME:-$HOME/lp0008-phase0/rc3_faucet_wallet}"
WALLET_CLI="${LP0008_RC3_WALLET_CLI:-$HOME/Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet}"
LOG="$HOME/lp0008-phase0/three_live_agents_setup_$TS.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee "$LOG") 2>&1

# These are public account identifiers, not private keys. Override all three via env for a different funded wallet.
ALPHA_ACCOUNT="${LP0008_ALPHA_ACCOUNT:-Private/5ya25h4Xc9GAmrGB2WrTEnEWtQKJwRwQx3Xfo2tucNcE}"
BETA_ACCOUNT="${LP0008_BETA_ACCOUNT:-Private/E8HwiTyQe4H9HK7icTvn95HQMnzx49mP9A2ddtMLpNaN}"
GAMMA_ACCOUNT="${LP0008_GAMMA_ACCOUNT:-Private/27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq}"

need() { [ -e "$1" ] || { echo "missing $1" >&2; exit 1; }; }
need "$WALLET_HOME/storage.json"
need "$WALLET_HOME/wallet_config.json"
need "$WALLET_CLI"

printf 'LP-0008 three live agent setup\n'
printf 'timestamp=%s\nroot=%s\nrepo_sha=%s\nstate_root=%s\nwallet_home=%s\n' "$TS" "$ROOT" "$(git rev-parse HEAD)" "$STATE_ROOT" "$WALLET_HOME"

printf '\n== build module and cabi caller ==\n'
nix build .#install --out-link result >/dev/null
mkdir -p .demo-state
nix develop --command c++ -std=c++17 -O2 -o .demo-state/cabi_call tests/cabi_call.cpp >/dev/null
PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.dylib"
[ -f "$PLUGIN" ] || PLUGIN="$ROOT/result/modules/agent_module/agent_module_plugin.so"
need "$PLUGIN"
need "$ROOT/.demo-state/cabi_call"

rm -rf "$STATE_ROOT"
mkdir -p "$STATE_ROOT"

write_agent() {
  local slug="$1" name="$2" desc="$3" account="$4" category="$5"
  local dir="$STATE_ROOT/$slug"
  mkdir -p "$dir"
  cp -R "$WALLET_HOME" "$dir/wallet"
  local account_raw="${account#Private/}"
  local account_raw="${account_raw#Public/}"
  python3 - "$dir/config.json" "$slug" "$name" "$desc" "$account_raw" "$category" <<'PY'
import json, sys
path, slug, name, desc, account, category = sys.argv[1:]
json.dump({
  "agent_id": slug,
  "agent_name": name,
  "description": desc,
  "owner_topic": f"/lp0008/strict/{slug}/owner",
  "task_topic": f"/lp0008/strict/{slug}/tasks",
  "wallet_account_hex": account,
  "sequencer_addr": "https://testnet.lez.logos.co/",
  "per_tx_limit": "1000",
  "per_period_limit": "10000",
  "period_seconds": "86400",
  "a2a_payment_recipient": account,
  "a2a_payment_amount_le16": "01000000000000000000000000000000",
  "strict_category": category
}, open(path, 'w'), indent=2, sort_keys=True)
PY
}

write_agent alpha-storage "Alpha Storage Agent" "Default storage/data-vault skill category agent" "$ALPHA_ACCOUNT" storage
write_agent beta-messaging "Beta Messaging Agent" "Default messaging/A2A coordination skill category agent" "$BETA_ACCOUNT" messaging
write_agent gamma-chain "Gamma Chain Agent" "Default wallet/program/blockchain skill category agent" "$GAMMA_ACCOUNT" chain

cat > "$STATE_ROOT/agents.json" <<JSON
{
  "created_at": "$TS",
  "repo_sha": "$(git rev-parse HEAD)",
  "wallet_home_source": "$WALLET_HOME",
  "wallet_cli": "$WALLET_CLI",
  "agents": [
    {"agent_id":"alpha-storage","category":"storage","state_dir":"$STATE_ROOT/alpha-storage","account":"$ALPHA_ACCOUNT"},
    {"agent_id":"beta-messaging","category":"messaging","state_dir":"$STATE_ROOT/beta-messaging","account":"$BETA_ACCOUNT"},
    {"agent_id":"gamma-chain","category":"chain","state_dir":"$STATE_ROOT/gamma-chain","account":"$GAMMA_ACCOUNT"}
  ]
}
JSON

python3 scripts/verify_three_live_agents.py --state-root "$STATE_ROOT" --wallet-cli "$WALLET_CLI" --log "$LOG"

echo "THREE_LIVE_AGENTS_STATE=$STATE_ROOT"
echo "THREE_LIVE_AGENTS_LOG=$LOG"
