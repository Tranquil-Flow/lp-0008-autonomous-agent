# LP-0008 strict live gate inventory

Generated: 20260630_180917 UTC
Repo SHA: `3c9203fb2c893479d362e21c88517ba3ade04aba`
Raw log: `$LP0008_TEST_ROOT/strict_live_gate_preflight_20260630_180917.log`
JSON: `$LP0008_TEST_ROOT/strict_live_gate_preflight_20260630_180917.json`

Counts: PASS=14 WARN=3 FAIL=0

| Status | Required | Gate | Detail |
|---|---|---|---|
| PASS | required | git | /usr/bin/git |
| PASS | required | python3 | /opt/homebrew/bin/python3 |
| PASS | required | nix | /nix/var/nix/profiles/default/bin/nix |
| PASS | required | lgs | $HOME/.cargo/bin/lgs |
| PASS | recommended | curl | /usr/bin/curl |
| PASS | recommended | nc | /usr/bin/nc |
| PASS | required | logoscore | $LP0008_TEST_ROOT/logoscore-cli/bin/logoscore |
| PASS | required | rc3-wallet-binary | $HOME/Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet |
| PASS | required | funded-wallet-home | $LP0008_TEST_ROOT/rc3_faucet_wallet |
| PASS | required | basecamp-source-cache | $HOME/Library/Caches/logos-scaffold/repos/basecamp |
| PASS | required | basecamp-module-cache | $HOME/Library/Caches/logos-scaffold/basecamp |
| PASS | required | scaffold.toml | $REPO_ROOT/scaffold.toml |
| PASS | required | wallet-private-account-count | private_accounts=5 public_accounts=6 |
| WARN | required | wallet-check-health | rc=101; 
thread 'main' (10638829) panicked at $HOME/.cargo/git/checkouts/logos-execution-zone-6bae42d7c9cadfe7/cf3639d/wallet/src/cli/mod.rs:134:13:
Local ID for authenticated transfer program is different from remote
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrac |
| PASS | required | lez-sequencer-reachability | https://testnet.lez.logos.co reachable |
| WARN | recommended | screen-sharing-vnc | not listening on localhost:5900; needed only for remote GUI recording |
| WARN | required | github-token | hosts.yml exists but oauth_token not found |

Warnings do not close strict criteria. They identify live gates that must be solved, manually supplied, or explicitly bounded before final video.
