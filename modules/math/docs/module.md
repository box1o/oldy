# Math

Math is a standalone header-only module exposed as `woki::math`. Include
`<woki/math.hpp>` for the canonical facade or include leaf headers such as
`<woki/math/vec/vec3.hpp>` when a narrower dependency is preferred.

Freestanding C++ extension guests use `<woki/math/guest.hpp>`. That facade
excludes host-only I/O facilities and remains available under the same include
spelling in source and installed SDK trees.
