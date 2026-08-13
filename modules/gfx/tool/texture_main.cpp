#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <woki/config.hpp>
#include <woki/gfx/advanced.hpp>

namespace {
using namespace woki;

Result<std::string> ReadText(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open texture descriptor");
    std::ostringstream text;
    text << stream.rdbuf();
    return stream ? Ok(text.str()) : Result<std::string>(Err(ErrorCode::FileReadError, "unable to read texture descriptor"));
}

Result<void> WriteProduct(const std::filesystem::path& path, const asset::Product& product) {
    std::vector<std::byte> bytes;
    TRY_ASSIGN(bytes, asset::SerializeProduct(product));
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream || (!bytes.empty() && !stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
        return Err(ErrorCode::FileWriteError, "unable to write texture product");
    return Ok();
}
} // namespace

int main(const int argc, const char* const* argv) {
    if (argc != 5 || std::string_view(argv[1]) != "cook") {
        std::cerr << "usage: woki-texture cook <descriptor> <asset-root> <output>\n";
        return 2;
    }
    auto text = ReadText(argv[2]);
    if (!text) {
        std::cerr << text.error().Message() << '\n';
        return 1;
    }
    auto source = gfx::ParseTextureSource(*text);
    if (!source) {
        std::cerr << source.error().Message() << '\n';
        return 1;
    }
    auto mount = asset::DirectoryMount::Create(argv[3]);
    if (!mount) {
        std::cerr << mount.error().Message() << '\n';
        return 1;
    }
    auto vfs = createRef<asset::Vfs>();
    vfs->AddMount(*mount);
    gfx::TextureBuilder builder(vfs);
    asset::BuildContext context;
    auto document = config::Document::Parse(*text, argv[2], config::ParsePolicy{true, true, false, {}});
    const ContentHash source_hash = document ? config::CanonicalHash(*document, "texture", 1) : Sha256(*text);
    auto built = builder.Build({source->asset_id, source->source_uri, source_hash, {}, 2}, context, std::as_bytes(std::span(*text)));
    if (!built) {
        std::cerr << built.error().Message() << '\n';
        return 1;
    }
    if (auto written = WriteProduct(argv[4], *built); !written) {
        std::cerr << written.error().Message() << '\n';
        return 1;
    }
    return 0;
}
