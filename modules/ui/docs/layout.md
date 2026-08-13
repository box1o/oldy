# Layout

Layout uses a constraints-in, size-out protocol. A parent measures children, chooses its constrained size, and then places them. Measurements are cached by node dirtiness and exact input constraints.

Supported containers are row, column, stack, and equal-track grid. Flow layout supports fixed, automatic, percentage, and grow lengths; min/max constraints; margins; padding; gaps; cross-axis alignment; main-axis justification; and absolute children. Grid supports deterministic column spans. Stack overlays children, using `Align` horizontally and `Justify` vertically. Absolute nodes can anchor to any opposing edges or center on either axis.

```cpp
Stack(
    Editor().Grow(),
    Button("Close", close)
        .Absolute()
        .Top(8)
        .Right(8))
    .Width(Percent{1})
    .Height(Percent{1});
```

`Overflow::Clip` records a balanced clip. `Overflow::Scroll` additionally retains wheel offsets on its element, so rebuilding the declaration does not reset scrolling.

Text measurement is injected through `TextEngine`. `SimpleText` is a deterministic fallback for tests; production shaping belongs in a separate text backend.
