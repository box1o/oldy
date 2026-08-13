#pragma once

#include <memory>
#include <string>
#include <vector>

#include "key.hpp"
#include "event.hpp"
#include "style.hpp"
#include "component.hpp"
#include "display.hpp"

namespace woki::ui {

enum class Kind : u8 { Box, Text, Component, Portal };
enum class Role : u8 { None, Text, Button, Checkbox, Radio, Switch, Slider, Input, Dialog, List, Item };

struct Semantics {
    Role role{Role::None};
    std::string label;
    std::string value;
    bool focusable{};
    bool disabled{};
    bool selected{};
    bool checked{};
    i32 tab_index{};
};

class View {
public:
    explicit View(Kind kind = Kind::Box);
    View(View&&) noexcept = default;
    View& operator=(View&&) noexcept = default;
    View(const View&) = default;
    View& operator=(const View&) = default;

    View& Id(Key key);
    View& Styled(Style style);
    View& SemanticsOf(Semantics semantics);
    View& OnEvent(EventHandler handler);
    View& Image(ImageId image, ImageFit fit = ImageFit::Fill);
    View& Add(View child);

    template <typename... Items>
    View& Children(Items&&... children) {
        (Add(std::forward<Items>(children)), ...);
        return *this;
    }

    View& Width(Length value);
    View& Height(Length value);
    View& Size(f32 width, f32 height);
    View& MinSize(f32 width, f32 height);
    View& MaxSize(f32 width, f32 height);
    View& Aspect(f32 value);
    View& Grow(f32 factor = 1.0f);
    View& Padding(Inset value);
    View& Margin(Inset value);
    View& Gap(f32 value);
    View& Span(u32 columns);
    View& AlignItems(Align value);
    View& JustifyItems(Justify value);
    View& Background(Color value);
    View& Foreground(Color value);
    View& Hover(Color value);
    View& Pressed(Color value);
    View& Transitioned(Transition value);
    View& Opacity(f32 value);
    View& Radius(f32 value);
    View& Border(Stroke value);
    View& Shadowed(Shadow value);
    View& Overflowed(Overflow value);
    View& FontSize(f32 value);
    View& LineHeight(f32 value);
    View& Absolute();
    View& Top(f32 value);
    View& Right(f32 value);
    View& Bottom(f32 value);
    View& Left(f32 value);
    View& CenterX();
    View& CenterY();
    View& Center();

    [[nodiscard]] Kind Type() const {
        return kind_;
    }

    [[nodiscard]] Key Identity() const {
        return key_;
    }

    [[nodiscard]] const ui::Style& GetStyle() const {
        return style_;
    }

    [[nodiscard]] const ui::Semantics& GetSemantics() const {
        return semantics_;
    }

    [[nodiscard]] const EventHandler& Handler() const {
        return handler_;
    }

    [[nodiscard]] const std::shared_ptr<Component>& Instance() const {
        return component_;
    }

    [[nodiscard]] const std::string& Content() const {
        return content_;
    }

    [[nodiscard]] ImageId ImageResource() const {
        return image_;
    }

    [[nodiscard]] ImageFit ImageSizing() const {
        return image_fit_;
    }

    [[nodiscard]] const std::vector<View>& ChildViews() const {
        return children_;
    }

    [[nodiscard]] std::vector<View>& ChildViews() {
        return children_;
    }

    void SetContent(std::string content) {
        content_ = std::move(content);
    }

    void SetComponent(std::shared_ptr<Component> component) {
        component_ = std::move(component);
    }

private:
    Kind kind_;
    Key key_{};
    ui::Style style_{};
    ui::Semantics semantics_{};
    EventHandler handler_{};
    std::shared_ptr<Component> component_;
    std::string content_;
    ImageId image_{};
    ImageFit image_fit_{ImageFit::Fill};
    std::vector<View> children_;
};

View Box();
View Text(std::string text);
View Mount(std::shared_ptr<Component> component);
View Portal(View content);
View Grid(u32 columns);

template <typename... Items>
View Row(Items&&... children) {
    Style style;
    style.flow = Flow::Row;
    return Box().Styled(style).Children(std::forward<Items>(children)...);
}

template <typename... Items>
View Column(Items&&... children) {
    Style style;
    style.flow = Flow::Column;
    return Box().Styled(style).Children(std::forward<Items>(children)...);
}

template <typename... Items>
View Stack(Items&&... children) {
    Style style;
    style.flow = Flow::Stack;
    return Box().Styled(style).Children(std::forward<Items>(children)...);
}

} // namespace woki::ui
