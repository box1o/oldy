# Status

Implemented in the renderer-independent rewrite:

- temporary declarations and persistent keyed elements;
- component-local revision invalidation and reconciliation;
- granular layout, paint, semantics, and hit-test dirtiness;
- constraint and display-fragment caching;
- row, column, stack, grid, absolute, percentage, grow, margin, padding, gap, alignment, and justification;
- clipping, retained scrolling, viewport culling, focus traversal, pointer capture, and phased event routing;
- UTF-8 editing and injectable text measurement;
- renderer-neutral painting with borders, corners, shadows, text, and clips;
- interruptible color and scalar motion tracks plus automatic hover/pressed transitions;
- JSONC colors, spacing, radii, typography, motion, and transactional reload;
- flat controls, inputs, dialogs, sheets, docking persistence, and JSON diagnostics;
- conversions to standalone math vector types;
- split subsystem tests and strict display validation.

Deliberately outside the renderer-neutral UI internals:

- production font shaping, bidi, multi-font fallback, and scalable glyph atlases (the `ui::render` integration supplies a deterministic ASCII/UTF-8 replacement baseline);
- OS accessibility and IME adapters;
- image and path resource backends;
- application-specific dock drag visuals and timeline models.

Those integrations depend on platform or renderer contracts. Their extension points are present, but claiming their platform behavior without those backends would be misleading.
