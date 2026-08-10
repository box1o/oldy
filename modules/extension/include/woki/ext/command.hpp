#pragma once

#include <string>

namespace woki::ext {

struct CommandContribution {
    std::string id;
    std::string title;
    std::string category;
};

struct CommandRecord {
    std::string extension_id;
    CommandContribution command;
};

} // namespace woki::ext
