#include <new>
#include <sstream>
#include <yaml-cpp/yaml.h>

#include "woki/config/config.hpp"

namespace woki {

namespace {

void FlattenYamlNode(const YAML::Node& node, std::string_view prefix, Config& config) {
    if (!node.IsDefined() || node.IsNull()) {
        return;
    }

    if (node.IsScalar()) {
        config.Set(prefix, node.as<std::string>());
        return;
    }

    if (node.IsMap()) {
        for (const auto& entry : node) {
            const auto key = entry.first.as<std::string>();
            std::string child_prefix;
            if (prefix.empty()) {
                child_prefix = key;
            } else {
                child_prefix = std::string(prefix) + "." + key;
            }
            FlattenYamlNode(entry.second, child_prefix, config);
        }
        return;
    }

    if (node.IsSequence()) {
        std::ostringstream stream;
        for (std::size_t index = 0; index < node.size(); ++index) {
            if (index > 0) {
                stream << ',';
            }
            stream << node[index].as<std::string>();
        }
        config.Set(prefix, stream.str());
    }
}

} // namespace

Result<Config> Config::LoadFromYamlFile(const std::filesystem::path& path) {
    Config config;
    auto loaded = config.LoadYaml(path);
    if (!loaded) {
        return Err(loaded.error());
    }
    return Ok(std::move(config));
}

void Config::Set(std::string_view key, std::string_view value) {
    values_[std::string(key)] = std::string(value);
}

bool Config::Has(std::string_view key) const {
    return values_.contains(std::string(key));
}

void Config::Remove(std::string_view key) {
    values_.erase(std::string(key));
}

void Config::Clear() {
    values_.clear();
}

std::size_t Config::Size() const noexcept {
    return values_.size();
}

bool Config::Empty() const noexcept {
    return values_.empty();
}

Result<std::string_view> Config::GetStringView(std::string_view key) const {
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return Err(ErrorCode::OutOfRange, "Config key not found");
    }

    return Ok(std::string_view(it->second));
}

Result<std::string> Config::GetString(std::string_view key) const {
    auto value = GetStringView(key);
    if (!value) {
        return Err(value.error());
    }

    return Ok(std::string(*value));
}

void Config::Merge(const Config& other) {
    for (const auto& [key, value] : other.values_) {
        values_[key] = value;
    }
}

Result<void> Config::LoadYaml(const std::filesystem::path& path) {
    try {
        const YAML::Node root = YAML::LoadFile(path.string());
        Config loaded;
        FlattenYamlNode(root, {}, loaded);
        Config merged = *this;
        merged.Merge(loaded);
        values_.swap(merged.values_);
        return Ok();
    } catch (const YAML::BadFile& exception) {
        return Err(ErrorCode::FileNotFound, exception.what());
    } catch (const YAML::Exception& exception) {
        return Err(ErrorCode::ParseInvalidFormat, exception.what());
    } catch (const std::bad_alloc&) {
        return Err(ErrorCode::FailedToAcquireResource, "Insufficient memory to load YAML config");
    }
}

} // namespace woki
