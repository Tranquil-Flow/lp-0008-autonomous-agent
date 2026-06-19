# Delivery receive/poll API gap for LP-0008

LP-0008 inspected the current `logos-delivery-module` API while hardening `agent.receive`.

Findings:

- messageReceived events exist in `DeliveryModulePlugin` and are emitted from the underlying liblogosdelivery callback.
- The public Q_INVOKABLE interface exposes `createNode`, `start`, `stop`, `send`, `subscribe`, `unsubscribe`, and node/config inspection methods.
- There is no synchronous receive/poll query API that an independent module can call through `logos::LpClient` to retrieve pending messages.

Current LP-0008 behavior:

- `messaging.send` uses live `delivery_module.send` for valid LIP-23 content topics.
- agent.receive remains persisted-inbox polling: task envelopes addressed to the configured `task_topic` are stored, then `agent.receive` ingests unprocessed inbox records through the same persisted A2A task lifecycle.
- This is intentionally described as persisted inbox processing, not as a background live delivery subscriber.

Requested upstream shape:

1. Add a bounded `pollReceived(contentTopic, limit)` or equivalent query method that returns buffered `messageReceived` events for a subscribed topic.
2. Preserve the existing asynchronous event flow for UI consumers.
3. Make the query method safe for headless Logos Core modules using `logos::LpClient`.
4. Return structured JSON with message hash, content topic, base64 payload, timestamp, and processed cursor/offset metadata.

This would let LP-0008 replace persisted loopback inbox records with real delivery receive polling while keeping the same `agent.receive` lifecycle.
