# LP-0008 final video audio narration

This narration follows the scene order in `scripts/record_final_video.sh`. Keep it concise and evidence-bound.

## Scene 1 — Introduction

"This is LP-0008, an autonomous AI agent module for Logos Core. I will show the repository state, the strict evidence gate, the live/bounded runtime paths, and the exact upstream/tooling limits that remain. The goal is not to hide blockers; it is to make every claim traceable."

## Scene 2 — Strict evidence gate

"This section runs `scripts/run_final_strict_evidence.sh`. The important detail is honesty: every non-video evidence step must either pass or be recorded as an explicit upstream/tooling blocker. Today the expected status is `ok_with_blockers`, with `paid-a2a-live` blocked by the rc3 wallet transfer proof panic after live Logos Messaging A2A completion."

## Scene 3 — Build and module load

"The module builds through the Logos module-builder/Nix path and loads through the C ABI. Linux CI verifies non-wallet behavior, while Darwin-hosted evidence exercises the wallet FFI path. Unsupported wallet platforms fail closed rather than pretending to send funds."

## Scene 4 — A2A card and task lifecycle

"The Agent Card advertises A2A protocol fields, Logos identity authentication, LEZ payment metadata, and the lifecycle skills. The task evidence shows queued, transport_sent, working, and completed states over the persisted task envelope."

## Scene 5 — Three use cases

"The three use cases are a personal file vault, an agent-services marketplace payment hook, and a multi-agent workflow. Payment-hook recording is shown as lifecycle evidence; confirmed autonomous LEZ payment binding remains a separate blocked row until wallet/tooling support is stable."

## Scene 6 — Owner approval and resilience

"Below-threshold actions can proceed autonomously. Above-threshold actions create persisted approvals, notify the owner channel, and fail closed on rejection, timeout, or stale retry. The evidence also shows that a failed skill does not poison unrelated storage or messaging paths."

## Scene 7 — Basecamp / owner surface

"The repository includes Basecamp-loadable metadata and owner-channel approval evidence. Any visual Basecamp step shown here is the reviewer-facing owner surface; terminal-only evidence is not described as a GUI proof."

## Scene 8 — Limits and final status

"The remaining non-video limitations are explicit: paid-A2A LEZ transfer binding is blocked by current wallet proof behavior, and in-process program deploy/call/CU evidence waits on a stable Logos Core LEZ program SDK or C ABI. The final submission still requires this narrated video URL and explicit approval before opening the public PR."
