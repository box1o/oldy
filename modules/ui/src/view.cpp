#include <utility>

#include <woki/ui/view.hpp>

namespace woki::ui {

View::View(Kind kind)
    : kind_(kind) {}

View& View::Id(Key key) {
    key_ = key;
    return *this;
}

View& View::Styled(Style style) {
    style_ = std::move(style);
    return *this;
}

View& View::SemanticsOf(Semantics semantics) {
    semantics_ = std::move(semantics);
    return *this;
}

View& View::OnEvent(EventHandler handler) {
    handler_ = std::move(handler);
    return *this;
}

View& View::Image(ImageId image, ImageFit fit) {
    image_ = image;
    image_fit_ = fit;
    return *this;
}

View& View::Add(View child) {
    children_.push_back(std::move(child));
    return *this;
}

View& View::Width(Length value) {
    style_.width = value;
    return *this;
}

View& View::Height(Length value) {
    style_.height = value;
    return *this;
}

View& View::Size(f32 width, f32 height) {
    return Width(Px{width}).Height(Px{height});
}

View& View::MinSize(f32 width, f32 height) {
    style_.min_width = std::max(0.0f, width);
    style_.min_height = std::max(0.0f, height);
    return *this;
}

View& View::MaxSize(f32 width, f32 height) {
    style_.max_width = std::max(0.0f, width);
    style_.max_height = std::max(0.0f, height);
    return *this;
}

View& View::Aspect(f32 value) {
    style_.aspect = std::max(0.0f, value);
    return *this;
}

View& View::Grow(f32 factor) {
    style_.width = ui::Grow{factor};
    style_.height = ui::Grow{factor};
    return *this;
}

View& View::Padding(Inset value) {
    style_.padding = value;
    return *this;
}

View& View::Margin(Inset value) {
    style_.margin = value;
    return *this;
}

View& View::Gap(f32 value) {
    style_.gap = value;
    return *this;
}

View& View::Span(u32 columns) {
    style_.span = std::max(1u, columns);
    return *this;
}

View& View::AlignItems(Align value) {
    style_.align = value;
    return *this;
}

View& View::JustifyItems(Justify value) {
    style_.justify = value;
    return *this;
}

View& View::Background(Color value) {
    style_.background = value;
    return *this;
}

View& View::Foreground(Color value) {
    style_.foreground = value;
    return *this;
}

View& View::Hover(Color value) {
    style_.hover_background = value;
    return *this;
}

View& View::Pressed(Color value) {
    style_.pressed_background = value;
    return *this;
}

View& View::Transitioned(Transition value) {
    style_.transition = value;
    return *this;
}

View& View::Opacity(f32 value) {
    style_.opacity = std::clamp(value, 0.0f, 1.0f);
    return *this;
}

View& View::Radius(f32 value) {
    style_.radius = ui::Radius::All(value);
    return *this;
}

View& View::Border(Stroke value) {
    style_.border = value;
    return *this;
}

View& View::Shadowed(Shadow value) {
    style_.shadow = value;
    return *this;
}

View& View::Overflowed(Overflow value) {
    style_.overflow = value;
    return *this;
}

View& View::FontSize(f32 value) {
    style_.font_size = std::max(0.0f, value);
    return *this;
}

View& View::LineHeight(f32 value) {
    style_.line_height = std::max(0.0f, value);
    return *this;
}

View& View::Absolute() {
    style_.position = Position::Absolute;
    return *this;
}

View& View::Top(f32 value) {
    style_.top = value;
    return *this;
}

View& View::Right(f32 value) {
    style_.right = value;
    return *this;
}

View& View::Bottom(f32 value) {
    style_.bottom = value;
    return *this;
}

View& View::Left(f32 value) {
    style_.left = value;
    return *this;
}

View& View::CenterX() {
    style_.center_x = true;
    return *this;
}

View& View::CenterY() {
    style_.center_y = true;
    return *this;
}

View& View::Center() {
    return CenterX().CenterY();
}

View Box() {
    return View{Kind::Box};
}

View Text(std::string text) {
    View view{Kind::Text};
    view.SetContent(std::move(text));
    Semantics semantics;
    semantics.role = Role::Text;
    view.SemanticsOf(std::move(semantics));
    return view;
}

View Mount(std::shared_ptr<Component> component) {
    View view{Kind::Component};
    view.SetComponent(std::move(component));
    return view;
}

View Portal(View content) {
    View view{Kind::Portal};
    view.Add(std::move(content));
    return view;
}

View Grid(u32 columns) {
    Style style;
    style.flow = Flow::Grid;
    style.columns = std::max(1u, columns);
    return Box().Styled(style);
}

} // namespace woki::ui
