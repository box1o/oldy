# Core

Core provides dependency-light process, memory, error, hashing, generic data primitives, and bounded task execution. Public data utilities include strongly tagged monotonic versions, generational slot maps, sparse-to-dense handle storage, merged dirty ranges, and bounded dirty bitsets. Task APIs retain the `<woki/task.hpp>` include and `woki::task` namespace.

Core data headers do not depend on graphics, assets, RHI, or math. Domain modules own policy and state while delegating generic identity, storage, and change tracking to these primitives.
