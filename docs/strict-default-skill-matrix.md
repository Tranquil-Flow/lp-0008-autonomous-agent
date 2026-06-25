# Strict default-skill matrix evidence

- Evidence dir: `/Users/evinova-self/lp0008-phase0/strict_skill_matrix_20260625_104715`
- Registered skills: 27
- Expected skills tested: 27

| Skill | Success / bounded behavior | Fail-closed / negative behavior |
|---|---|---|
| `agent.cancel` | success-terminal-task-refused-safely | failure-not-found |
| `agent.card` | success-card | not applicable / read-only |
| `agent.complete` | success-complete-existing | failure-not-found |
| `agent.discover` | success-self-published | not applicable / read-only |
| `agent.receive` | success-process-inbound | failure-malformed-envelope-processed |
| `agent.subscribe` | success-subscribe-completed | failure-not-found |
| `agent.task` | success-completed-live-transport | failure-unknown-skill |
| `approval.approve` | success-execute-approved | failure-not-pending, failure-not-found |
| `approval.list` | success-list | not applicable / read-only |
| `approval.reject` | success-reject | failure-not-pending |
| `approval.retry` | success-retry-notifies | failure-not-pending |
| `messaging.create_group` | success-create-group | failure-non-json-members-normalized |
| `messaging.join` | success-group-join | failure-invalid-group-fallback |
| `messaging.send` | success-live-send, concurrency-six-live-sends | failure-invalid-topic-fallback |
| `meta.configure` | success-agent_id, success-agent_name, success-task_topic, success-owner_topic, success-per_tx_limit, success-period_seconds | failure-invalid-key |
| `meta.skills` | success-list-all | not applicable / read-only |
| `meta.status` | success-status, resilience-modules-loaded-after-failures | not applicable / read-only |
| `program.call` | read-only covered by bounded status | failure-live-call-unavailable |
| `program.deploy` | read-only covered by bounded status | failure-missing-binary, failure-live-deploy-unavailable |
| `program.query` | success-bounded-query | not applicable / read-only |
| `storage.download` | success-download | failure-address-not-found |
| `storage.list` | success-list | not applicable / read-only |
| `storage.share` | success-share-recorded | failure-address-not-found |
| `storage.upload` | success-live-upload | failure-missing-file |
| `wallet.balance` | success-balance | not applicable / read-only |
| `wallet.history` | success-history | not applicable / read-only |
| `wallet.send` | success-below-limit | failure-above-limit-requires-approval |

## Concurrency and resilience

- Six parallel live `messaging.send` calls returned distinct live message IDs.
- Invalid topic and fail-closed cases did not unload `agent_module`, `storage_module`, or `delivery_module`.
- Storage share/group semantics are explicitly covered by success and missing/invalid cases.
