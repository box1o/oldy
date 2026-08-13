#include <algorithm>

#include <woki/ui/text/edit.hpp>

namespace woki::ui {

Edit::Edit(std::string value) {
    Set(std::move(value));
}

void Edit::Set(std::string value) {
    value_ = std::move(value);
    cursor_ = anchor_ = value_.size();
}

void Edit::Select(size_t anchor, size_t cursor) {
    anchor_ = std::min(anchor, value_.size());
    cursor_ = std::min(cursor, value_.size());
}

void Edit::SelectAll() {
    anchor_ = 0;
    cursor_ = value_.size();
}

void Edit::Move(i32 direction, bool extend) {
    const size_t next = direction < 0 ? Previous(cursor_) : Next(cursor_);
    cursor_ = next;
    if (!extend)
        anchor_ = cursor_;
}

void Edit::Home(bool extend) {
    cursor_ = 0;
    if (!extend)
        anchor_ = cursor_;
}

void Edit::End(bool extend) {
    cursor_ = value_.size();
    if (!extend)
        anchor_ = cursor_;
}

void Edit::Insert(std::string_view text) {
    EraseSelection();
    value_.insert(cursor_, text);
    cursor_ += text.size();
    anchor_ = cursor_;
}

void Edit::Backspace() {
    if (Selected())
        return EraseSelection();
    const size_t previous = Previous(cursor_);
    value_.erase(previous, cursor_ - previous);
    cursor_ = anchor_ = previous;
}

void Edit::Delete() {
    if (Selected())
        return EraseSelection();
    value_.erase(cursor_, Next(cursor_) - cursor_);
    anchor_ = cursor_;
}

void Edit::EraseSelection() {
    const size_t first = std::min(anchor_, cursor_);
    const size_t last = std::max(anchor_, cursor_);
    value_.erase(first, last - first);
    cursor_ = anchor_ = first;
}

size_t Edit::Previous(size_t position) const {
    if (position == 0)
        return 0;
    --position;
    while (position > 0 && (static_cast<u8>(value_[position]) & 0xC0u) == 0x80u)
        --position;
    return position;
}

size_t Edit::Next(size_t position) const {
    if (position >= value_.size())
        return value_.size();
    ++position;
    while (position < value_.size() && (static_cast<u8>(value_[position]) & 0xC0u) == 0x80u)
        ++position;
    return position;
}

} // namespace woki::ui
