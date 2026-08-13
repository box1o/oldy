# UI

`woki::ui` is a retained, renderer-independent UI runtime. Applications create inexpensive `View` declarations. The runtime reconciles them into persistent `Element` nodes, performs cached constraint layout, routes input, advances motion, and records an immutable `DisplayList`.

```cpp
#include <woki/ui.hpp>

using namespace woki::ui;

Runtime runtime;

runtime.SetContent(Column(
    Text("Project"),
    Button("Export", export_project),
    Input(name, "Name"))
    .Padding(Inset::All(12))
    .Gap(8));

runtime.HandleEvent(event);
runtime.Prepare({.viewport = {width, height}, .scale = scale, .time = now});
renderer.Upload(runtime.Display());
```

Built-in widgets use `Theme::Default()` automatically. To override a widget,
pass a custom theme as its final argument, for example
`Button("Export", export_project, Tone::Primary, ControlSize::Medium, theme)`.

The UI module never creates GPU resources and does not include GFX headers. A renderer adapter consumes display operations separately.
