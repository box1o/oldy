# Components

Stateless components are functions returning `View`:

```cpp
View Toolbar(std::function<void()> save) {
    return Row(
        Text("Editor"),
        Box().Width(Grow{}),
        Button("Save", std::move(save)))
        .AlignItems(Align::Center)
        .Gap(8);
}
```

Retained components inherit `Component`:

```cpp
class Counter final : public Component {
public:
    void Increment() {
        ++value_;
        Invalidate();
    }

    View Build() override {
        return Button(std::to_string(value_), [this] { Increment(); })
            .Id(Key::From("counter.button"));
    }

private:
    int value_{};
};
```

Mount it with `Mount(component)`. Only that retained component subtree reconciles after invalidation.

Built-in controls follow the shadcn source-ownership model. They are small compositions of `Box`, `Text`, layout, semantics, and event handlers. Applications can wrap or copy them without extending the runtime.
