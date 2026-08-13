#pragma once

#include <string>
#include <string_view>

#include "../event.hpp"

namespace woki::ui {

class Edit {
public:
    explicit Edit(std::string value = {});

    void Set(std::string value);
    void Select(size_t anchor, size_t cursor);
    void SelectAll();
    void Move(i32 direction, bool extend = false);
    void Home(bool extend = false);
    void End(bool extend = false);
    void Insert(std::string_view text);
    void Backspace();
    void Delete();

    [[nodiscard]] const std::string& Value() const {
        return value_;
    }

    [[nodiscard]] size_t Cursor() const {
        return cursor_;
    }

    [[nodiscard]] size_t Anchor() const {
        return anchor_;
    }

    [[nodiscard]] bool Selected() const {
        return cursor_ != anchor_;
    }

private:
    void EraseSelection();
    [[nodiscard]] size_t Previous(size_t position) const;
    [[nodiscard]] size_t Next(size_t position) const;

    std::string value_;
    size_t cursor_{};
    size_t anchor_{};
};

} // namespace woki::ui
