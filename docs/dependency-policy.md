# LP-0008 dependency policy

`metadata.json` keeps dependencies as [] intentionally.

Why:

1. Logos Core refused progressive standalone loading when `agent_module` declared hard dependencies that were not present.
2. LP-0008 must remain usable as a single loadable Logos Core module for reviewers running the default demo.
3. Storage and Delivery are optional runtime clients: the agent creates `logos::LpClient` connections to `storage_module` and `delivery_module` when those modules are co-loaded, and falls back only for invalid/local/offline paths.

This is not a claim that integration is skipped. The full-stack co-load is verified by `scripts/run_logoscore_integration.sh`, which builds and loads agent + storage + delivery together, exercises live storage upload/list and valid LIP-23 delivery send, and preserves guarded local handling for malformed topics.

Policy:

- Keep manifest dependencies empty for standalone/progressive loading.
- Treat storage and delivery as optional runtime clients.
- Keep docs explicit: "metadata.json keeps dependencies as []" and "full-stack co-load is verified" are both true.
- Do not restore hard manifest dependencies unless Logos Core dependency resolution can load standalone and co-loaded variants reliably.

Reviewer summary: `metadata.json` remains dependency-light for portability; runtime integration is proven by explicit full-stack harnesses rather than by brittle manifest coupling.
