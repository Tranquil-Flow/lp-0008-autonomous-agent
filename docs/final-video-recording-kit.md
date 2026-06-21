# LP-0008 final video recording kit

This kit is for the final terminal-only LP-0008 demo. It does not push to
`logos-co/lambda-prize` and does not open any PR.

## Claim boundary

Show these claims only:

- The agent module builds and loads through Logos Core/C ABI paths.
- The raw harness verifies all 23 skills.
- Storage and delivery live paths are exercised where module APIs exist.
- A2A task intake uses `agent.receive` with persisted inbox polling.
- Wallet balance/history and funded wallet-send use live LEZ wallet FFI against public testnet when a funded rc3 wallet is mounted.
- Program query is simulated; `program.call` and `program.deploy` fail closed because Logos Core exposes no module-safe LEZ program SDK/C ABI yet.
- Optional Basecamp CLI readiness may be shown by displaying `scaffold.toml`, `lgs basecamp modules --show`, and `nix build .#lgx`; this is artifact readiness, not GUI interaction proof.

Do not claim GUI/Basecamp evidence or live program execution unless a real Basecamp launch/install session is separately recorded.

## Recording command

Run this from the laptop terminal. It records remote M4 output over SSH:

```bash
ssh -t m4pro 'bash -lc '\''
set -euo pipefail
PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH
cd ~/Projects/logos-basecamp/lp-0008-autonomous-agent

echo "## LP-0008 Autonomous AI Agent Module"
echo "## Public implementation branch"
git status -sb
git log -1 --oneline
printf "remote branch: "
git ls-remote origin refs/heads/feat/real-lez-wallet | cut -f1

echo "## Build"
nix build .#install --out-link result

echo "## Optional Basecamp CLI readiness"
sed -n '1,80p' scaffold.toml
lgs basecamp modules --show
nix build .#lgx --out-link result-lgx

echo "## Core demo"
bash demo.sh

echo "## Logos Core integration"
./scripts/run_logoscore_integration.sh

echo "## Multi-agent A2A receive demo"
./scripts/run_multi_agent_a2a_demo.sh

echo "## Deep verification"
./scripts/run_lp0008_deep_verify.py

echo "## Acceptance readiness"
./scripts/validate_acceptance_readiness.py

echo "## Optional live wallet send proof"
echo "This spends public-testnet LEZ from the mounted funded rc3 wallet."
LP0008_LIVE_WALLET_HOME=~/lp0008-phase0/rc3_faucet_wallet \
LP0008_LIVE_WALLET_ACCOUNT=27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq \
LP0008_LIVE_WALLET_AMOUNT=1 \
./scripts/run_live_wallet_send_verify.py
'\'''
```

If the optional wallet proof is too slow during recording, skip only that block and show the latest retained command output from `~/lp0008-phase0/` after the video, or rerun it in a short second clip.

## Narration beats

1. "This is LP-0008, an autonomous AI agent module for Logos Core. The implementation branch is public on Tranquil-Flow/lp-0008-autonomous-agent."
2. "The demo starts from a clean tracked tree and shows the exact commit being submitted."
3. "The build uses the module-builder/Nix path and produces the agent module artifact."
4. "The core demo calls the raw C ABI harness, avoiding the logoscore JSON display quirk."
5. "Integration loads agent, storage, and delivery together through Logos Core."
6. "The A2A demo proves three configured agents, agent cards, task intake, subscribe readback, and terminal cancel guards."
7. "Deep verification exercises all 23 skills and checks the wallet account reported by meta.status matches wallet.balance."
8. "The live wallet verifier mounts an rc3 Pinata-funded private wallet, submits wallet.send through the module FFI, confirms the transaction on public testnet, and observes balance decrease."
9. "Program execution is not faked: call and deploy fail closed until Logos Core exposes a module-safe LEZ program SDK/C ABI."
10. "The readiness validator checks that these claim boundaries stay documented."

## After upload

1. Copy the final video URL.
2. Patch `SUBMISSION.md` in this repo, replacing the pending video line.
3. Rerun:

```bash
./scripts/validate_acceptance_readiness.py
./scripts/run_lp0008_deep_verify.py
```

4. Commit and push the video-URL update to `Tranquil-Flow/lp-0008-autonomous-agent`.
5. Only after explicit approval, prepare a minimal `solutions/LP-0008.md` branch for `logos-co/lambda-prize`.
