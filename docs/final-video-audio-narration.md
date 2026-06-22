# LP-0008 Final Video Audio Narration Script

Use this narration while recording `scripts/record_final_video.sh`. It follows that script's scene order exactly. Keep claims bounded: this is strong terminal/package evidence, not a Basecamp GUI-owner-channel claim and not a proof-mode LEZ program execution claim.

## LP-0008 Autonomous AI Agent for Logos Core

"This is LP-0008, an autonomous AI agent module for Logos Core. The agent has a documented skill registry, file-backed state, spending controls, storage and messaging integration, A2A-compatible Agent Cards and tasks, and a LEZ wallet bridge where the public-testnet wallet FFI is available. This recording shows the reproducible evidence bundle from the public implementation repository."

## Repository and public commit

"First I am showing the exact repository state and commit. This is the implementation repository that reviewers can clone. The working tree should be clean, and the commit shown here is the public commit that CI verifies."

## Nix module build

"Now I build the install artifact with Nix. This produces the loadable Logos Core module package used by the demos and integration scripts."

## Basecamp CLI artifact readiness

"This scene shows Basecamp CLI artifact readiness: the scaffold metadata and LGX package are present and buildable. I am not claiming this terminal scene proves a separate Basecamp GUI owner-channel conversation; it proves the packaged artifact path and module visibility used for Basecamp integration."

## Core C ABI demo

"The core demo uses the direct C ABI harness rather than relying on CLI display formatting. It exercises the dispatch surface and validates structured JSON results, including configuration, storage, messaging, wallet gates, program boundary behavior, A2A task lifecycle, and owner approval flow."

## Logos Core integration with storage and delivery

"Next is the Logos Core integration script. It loads the agent alongside storage and delivery modules and verifies the module works through Logos Core, not just the standalone harness. Storage upload and download paths are exercised where the module API supports them, and delivery uses valid Logos Messaging topics for live send paths."

## Live Logos Messaging A2A deep verifier

"The deep verifier proves A2A task transport over Logos Messaging. The important evidence is the task lifecycle containing transport_sent and a transport result whose mode is live when a valid LIP-23 task topic is configured. This is the privacy-preserving transport layer replacing vanilla A2A HTTP."

## Three configured agents and use cases

"Here the evidence bundle demonstrates three configured agents: Alpha Storage, Beta Messaging, and Gamma Chain. The multi-agent script verifies discovery, inbound task processing, subscription readback, and terminal-state cancellation behavior. The three-use-case script demonstrates a personal file vault, an agent services marketplace payment hook, and a multi-agent workflow. Where funded wallet environment variables are absent, payment-hook recording is deterministic rather than pretending a live inter-agent LEZ payment happened."

## Persistence and guarded resilience

"The resilience gate verifies that pending owner approvals survive process restart, expired approvals cannot execute, and a failing skill does not crash the module or poison later skill execution. This covers the reliability properties that matter for a long-running autonomous agent."

## Acceptance validator

"The validator checks reviewer-facing evidence and honesty boundaries. It is designed to fail if the submission text drifts into overclaiming, uses stale skill counts, or omits required evidence documents."

## Evidence map

"The evidence map summarizes what is proven and what is intentionally not claimed. Live public-testnet wallet evidence is retained by transaction hash. Program call and deploy return bounded errors until Logos exposes a module-safe LEZ program SDK or C ABI, and CU/proof-mode limitations are documented rather than hidden."

## Close

"The non-video evidence bundle is green. The remaining publication step is to upload this narrated video, insert its public URL into the submission text, rerun the validators, and then open the Lambda Prize PR only after explicit approval."
