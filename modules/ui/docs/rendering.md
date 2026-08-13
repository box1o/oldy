# Rendering

`Canvas` records rectangles, rounded rectangles, borders, shadows, text, opaque `ImageId` values, and clips. Commands contain only renderer-neutral values.

Each retained element caches the display fragment for its complete subtree. A clean subtree appends that fragment directly. Visibility culling happens before recording. Viewport changes invalidate visibility-dependent paint caches.

`woki::ui` directly integrates with `woki::gfx` at the `woki::ui::render` boundary. Renderer-neutral retained state and display lists remain independent of graphics details, while the owner-thread adapter tessellates immutable display lists, resolves logical images and glyphs through injected services, preserves command order, and folds clip stacks into batch scissors. Include `<woki/ui/render.hpp>` for this bridge.

`DisplayList::Valid` rejects negative or non-finite geometry and unbalanced clips before upload.
