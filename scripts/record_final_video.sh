#!/usr/bin/env bash
set -euo pipefail

RUN_COMMANDS=${RUN_COMMANDS:-1}
SECTION_PAUSE=${SECTION_PAUSE:-1}
COMMAND_PAUSE=${COMMAND_PAUSE:-1}

section() {
  printf '\n\n## %s\n' "$1"
  if [ "$SECTION_PAUSE" != "0" ]; then sleep "$SECTION_PAUSE"; fi
}

run() {
  printf '\n$ %s\n' "$*"
  if [ "$RUN_COMMANDS" = "1" ]; then
    "$@"
  fi
  if [ "$COMMAND_PAUSE" != "0" ]; then sleep "$COMMAND_PAUSE"; fi
}

run_shell() {
  printf '\n$ %s\n' "$1"
  if [ "$RUN_COMMANDS" = "1" ]; then
    bash -lc "$1"
  fi
  if [ "$COMMAND_PAUSE" != "0" ]; then sleep "$COMMAND_PAUSE"; fi
}

section "LP-0008 Autonomous AI Agent for Logos Core"
printf 'This recording shows the submitted implementation commit and the executable evidence bundle.\n'
printf 'Scope note: Basecamp artifact readiness is shown as CLI/package evidence; this terminal video does not claim GUI interaction.\n'

section "Repository and public commit"
run git status -sb
run git log -1 --oneline
run_shell "git ls-remote origin refs/heads/main refs/heads/feat/real-lez-wallet"

section "Final strict evidence umbrella"
printf 'This gate reruns all non-video evidence and records known upstream blockers explicitly. Expected status before upstream wallet/program fixes is ok_with_blockers, not silent success.\n'
run ./scripts/run_final_strict_evidence.sh

section "Nix module build"
run nix build .#install --out-link result
run_shell "find result/modules -maxdepth 2 -type f | sort"

section "Basecamp CLI artifact readiness"
run_shell "sed -n '1,80p' scaffold.toml"
run lgs basecamp modules --show
run nix build .#lgx --out-link result-lgx

section "Core C ABI demo"
run bash demo.sh

section "Logos Core integration with storage and delivery"
run ./scripts/run_logoscore_integration.sh all

section "Live Logos Messaging A2A deep verifier"
run ./scripts/run_lp0008_deep_verify.py

section "Three configured agents and use cases"
run ./scripts/run_multi_agent_a2a_demo.sh
run ./scripts/run_three_use_cases_demo.sh

section "Persistence and guarded resilience"
run ./scripts/run_resilience_evidence.sh

section "Acceptance validator"
run ./scripts/validate_acceptance_readiness.py

section "Evidence map"
run_shell "sed -n '1,180p' docs/strict-success-criteria-evidence.md"

section "Close"
printf 'The repository commit, build, Logos Core integration, live messaging transport, three-agent workflows, approval timeout behavior, and validator have all been shown from executable commands.\n'
printf 'After recording, add the public video URL to SUBMISSION.md and the Lambda Prize solution file, rerun the validator, then request explicit approval before opening a public prize PR.\n'
