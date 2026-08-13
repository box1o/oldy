#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

namespace woki::asset {
class ProductCache;
class BuilderRegistry;

struct AssetServices {
    Vfs* vfs{};
    AssetDatabase* database{};
    ProductCache* cache{};
    AssetManager* manager{};
    DependencyGraph* dependencies{};
    BuilderRegistry* builders{};
};
} // namespace woki::asset
