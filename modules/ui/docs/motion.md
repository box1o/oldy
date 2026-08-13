# Motion

Motion is keyed by element identity and property. Tracks support floating-point and color values, delays, interruption, and linear, in, out, and in-out curves.

Hover and pressed backgrounds are ordinary style states:

```cpp
Box()
    .Background(idle)
    .Hover(hover)
    .Pressed(active)
    .Transitioned({.duration = 120ms, .curve = Curve::Out});
```

Changing hover or pressed state marks paint only. The runtime samples active tracks each frame and never performs layout for a color transition. Layout properties can use explicit `Motion` tracks when an application intentionally wants animated reflow.
