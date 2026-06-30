# LP-0008 resilience evidence

This document maps persistence, fail-closed timeout, and failure-isolation evidence to the executable gate:

- `scripts/run_resilience_evidence.sh`

The script writes evidence under:

- `$LP0008_RESILIENCE_STATE` if set, otherwise `$LP0008_TEST_ROOT/resilience_<timestamp>/`

## What the gate proves

1. Pending approval persistence (pending approval persistence) across restart/process boundary
   - Configures a tiny per-transaction limit.
   - Calls `wallet.send` for a 10 LEZ-equivalent transfer.
   - Verifies the transfer is not executed and instead creates `approval_id` with `owner_approval_required`.
   - Calls `approval.list` in a new plugin process and verifies the pending approval reloads from `approvals.json`.

2. Unreachable-owner timeout/fail-closed behavior
   - Edits the persisted approval `expires_at` to a past timestamp to deterministically simulate timeout.
   - Calls `approval.retry` and verifies `approval_expired` / `timed_out`.
   - Calls `approval.approve` after timeout and verifies no execution occurs.
   - Re-lists approvals and verifies the `timed_out` status persisted.

3. Skill failure isolation
   - Calls `agent.task` with `agent.no_such_skill` and verifies the task status is `failed` with `unknown_skill`.
   - Immediately runs `storage.upload` and `storage.download` and byte-compares the result.
   - This proves one failed task does not poison later skill dispatch in the same persisted state.

## Evidence files

- `approval-request.out`
- `approval-list-after-restart.out`
- `approval-retry-expired.out`
- `approval-approve-after-timeout.out`
- `approval-list-after-timeout.out`
- `task-failure.out`
- `post-failure-upload.out`
- `post-failure-download.out`
- `summary.json`

## Success marker

A successful run prints:

`ASSERT resilience: pending approval persisted across restart; expired approval failed closed; failed task isolated from later storage skill: OK`

## Strict boundary

This is a deterministic local persistence and dispatch-isolation proof. It does not claim distributed consensus, full network-partition recovery, or Basecamp GUI owner-channel recovery. Those remain separate proof classes.
