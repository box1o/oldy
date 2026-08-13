# Architecture

The runtime has three distinct trees:

1. `View` is a copyable temporary declaration containing properties, children, semantics, and callbacks.
2. `Element` is retained across frames. It owns interaction state, component identity, layout results, dirty flags, and cached display fragments.
3. `DisplayList` is immutable renderer input for the prepared frame.

`Component::Build` returns a `View`. Calling `Invalidate` increments that component's revision. During frame preparation only invalidated component hosts rebuild, and their declarations are reconciled into the existing element subtree.

Children match by explicit key and kind. Unkeyed children match by kind and sibling position. Stable keys preserve element state through insertion and reorder. Removed elements release their paint and motion state.

Dirty work is divided into build, layout, paint, semantics, and hit-test categories. Layout dirtiness propagates through ancestors whose size may depend on the changed child. Paint dirtiness propagates only through the cached display-fragment chain. Paint-only state changes do not invalidate measurement.

The runtime is intentionally not a global singleton. Independent runtimes can coexist for windows, tests, previews, and offscreen documents.
