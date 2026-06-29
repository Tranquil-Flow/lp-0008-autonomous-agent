# Skill extension guide

LP-0008 exposes two extension paths:

1. Built-in module skills registered by `SkillRegistry` and dispatched through `dispatchSkill`.
2. External sidecar-agent skills addressed through the A2A task envelope over Logos Messaging.

The second path is the no-core-edit extension surface. A new service can publish an Agent Card and a `skill_plugin_manifest`, receive `agent.task` messages over Logos Messaging, execute its own code outside the core C++ agent module, and return task lifecycle events/results. That adds a skill provider without modifying core agent module dispatch.

`skill_plugin_manifest` minimum schema:

```json
{
  "schema": "lp0008.skill_plugin_manifest.v1",
  "name": "example.echo",
  "category": "extension",
  "description": "Example external skill contract",
  "input_schema": {"type": "object"},
  "output_schema": {"type": "object"},
  "transport": "A2A task envelope over Logos Messaging",
  "core_module_change_required": false
}
```

Evidence command:

```sh
scripts/run_skill_extension_evidence.py
```

Boundary: this guide does not claim hot-loaded in-process C++ plugins. It documents and verifies the sidecar-agent pattern: without modifying core agent module code, an external agent can add a new skill contract and interoperate through A2A over Logos Messaging.
