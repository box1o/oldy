#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include <woki/asset.hpp>

namespace {

class TempDirectory {
public:
    TempDirectory() {
        static std::size_t sequence = 0;
        path_ = std::filesystem::temp_directory_path() / ("woki-asset-tests-" + std::to_string(++sequence));
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("VFS reads directory and memory mounts in order") {
    TempDirectory directory;
    std::filesystem::create_directories(directory.Path() / "shaders");
    std::ofstream(directory.Path() / "shaders/lit.wgsl", std::ios::binary) << "directory";

    const auto disk = woki::asset::DirectoryMount::Create(directory.Path());
    const auto memory = woki::createRef<woki::asset::MemoryMount>();
    const auto path = woki::asset::AssetPath::Parse("shaders/lit.wgsl");
    const auto generated = woki::asset::AssetPath::Parse("generated/default.wgsl");
    REQUIRE(disk);
    REQUIRE(path);
    REQUIRE(generated);
    memory->PutText(*path, "memory");
    memory->PutText(*generated, "generated");

    woki::asset::Vfs vfs;
    vfs.AddMount(*disk);
    vfs.AddMount(memory);

    REQUIRE(vfs.ReadText(*path) == "directory");
    REQUIRE(vfs.ReadText(*generated) == "generated");
    REQUIRE_FALSE(vfs.ReadBinary(*path, 4));
}

TEST_CASE("Directory mounts reject symlink escapes") {
    TempDirectory root;
    TempDirectory outside;
    std::ofstream(outside.Path() / "secret", std::ios::binary) << "secret";
    std::error_code error;
    std::filesystem::create_directory_symlink(outside.Path(), root.Path() / "escape", error);
    if (error) {
        SKIP("directory symlinks are unavailable");
    }

    const auto mount = woki::asset::DirectoryMount::Create(root.Path());
    const auto path = woki::asset::AssetPath::Parse("escape/secret");
    REQUIRE(mount);
    REQUIRE(path);
    REQUIRE_FALSE((*mount)->Read(*path, 1024));
}
