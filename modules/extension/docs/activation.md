# Extension activation and capabilities

Extension manifests use explicit declarative activation:

```yaml
permissions:
  - log
  - events
activation:
  startup: true
  tick: true
  events:
    - window.resized
contributes:
  commands:
    - id: woki.example.run
      title: Run Example
```

`activation` is optional. If omitted, `startup` and `tick` are `false` and `events` is empty. This keeps existing manifests valid while changing them to lazy activation. Set the fields explicitly to preserve prior eager startup or ticking behavior.

Contributed commands are always indexed without loading their package and automatically activate their owning extension when executed. Command IDs are not repeated in `activation`.

On a named application event, the manager activates only packages listing that exact event. The event is delivered only when the active runtime session has also subscribed through `host_event_subscribe`; wildcard runtime subscriptions do not broaden the manifest declaration. Event activation requires the `events` permission. Deprecated high-bit numeric extension events do not activate packages, but are delivered to already active sessions with the `events` grant and a matching numeric or wildcard subscription. Other unknown numeric events are ignored.

`tick: true` opts an already active extension into frame ticks; it does not activate an extension by itself. Use `startup: true`, a command, or a declared application event as its activation trigger.

Manifest `permissions` are capability requests, represented as `RequestedCapabilities`. Before runtime creation, `CapabilityPolicy` resolves them to separate `EffectiveCapabilities` grants. The current `PermissiveCapabilityPolicy` grants every requested capability. Hosts can install another policy with `Manager::SetCapabilityPolicy` for future consent or administrative controls without changing manifests or guest SDK headers.

Supported application event names are defined by the manifest schema. They use stable names such as `window.resized`, `key.pressed`, and `app.resume` while retaining the existing numeric guest ABI.
