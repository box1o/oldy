# Extension activation and capabilities

Extension manifests use explicit declarative activation:

```yaml
permissions:
  - log
  - events
activation:
  startup: true
  tick: true
contributes:
  commands:
    - id: woki.example.run
      title: Run Example
```

`activation` is optional. If omitted, `startup` and `tick` are `false`. Set the fields explicitly for eager startup or ticking behavior.

Contributed commands are always indexed without loading their package and automatically activate their owning extension when executed. Command IDs are not repeated in `activation`.

The `events` manifest permission opts the package into every public application event supported by ABI v1. On the first such event, the manager activates an inactive package and delivers the event when the host capability policy grants `events`. Runtime subscription calls do not filter delivery. Deprecated high-bit numeric extension events and named extension events do not activate packages, but are delivered to every already active runtime with the effective `events` grant. Other unknown numeric events are ignored.

`tick: true` opts an already active extension into frame ticks; it does not activate an extension by itself. Use `startup: true`, a command, or the `events` permission as its activation trigger.

Manifest `permissions` are capability requests, represented as `RequestedCapabilities`. Before runtime creation, `CapabilityPolicy` resolves them to separate `EffectiveCapabilities` grants. The current `PermissiveCapabilityPolicy` grants every requested capability. Hosts can install another policy with `Manager::SetCapabilityPolicy` for future consent or administrative controls without changing manifests or guest SDK headers.

Supported public application events are defined by the ABI event schema and retain their stable numeric guest ABI.
