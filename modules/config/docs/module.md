# Configuration

`woki::config` is the sole parser and serializer boundary for authored and generated configuration. It provides an immutable, source-located DOM; bounded strict JSON and authored JSONC policies; a restricted private YAML adapter; typed nonthrowing views; structural schemas and migrations; and deterministic semantic hashes.

Authored JSONC requires a `$schema` URI. Legacy numeric `schema` fields are accepted only by an explicitly registered migration. Runtime and generated inputs use the strict policy. Exact source hashes identify bytes; `CanonicalHash` identifies migrated semantics and includes the schema family/version envelope.

`Registry::Maintained` defines the CI/tool acceptance window: the requested
version must be current or have a registered migration path to current.
`woki-config upgrade` is the explicit source upgrade command (with `migrate`
retained as its equivalent); migrations rewrite both `$schema` and `schema`.

YAML is intentionally narrower than YAML 1.2: only string-keyed maps, sequences, null, booleans, decimal integers, finite numbers, and strings are admitted. Aliases, anchors, tags, merge keys, and complex keys are rejected. `yaml-cpp` is a private implementation dependency.

`woki-config` supports `validate`, `migrate`, `canonicalize`, `schema`, and JSON Pointer `get` operations. Editor schemas and the catalog are emitted from the same in-process structural registry.
