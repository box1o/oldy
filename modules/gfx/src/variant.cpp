#include <woki/gfx/variant.hpp>

#include <charconv>
#include <limits>

namespace woki::gfx {
namespace {

std::string Encode(const PermutationValue& value) {
    return std::visit(
        [](const auto& scalar) {
            using T = std::decay_t<decltype(scalar)>;
            if constexpr (std::same_as<T, bool>)
                return std::string(scalar ? "b1" : "b0");
            else if constexpr (std::same_as<T, std::string>)
                return "s" + std::to_string(scalar.size()) + ":" + scalar;
            else {
                char buffer[64]{};
                const auto result = std::to_chars(std::begin(buffer), std::end(buffer), scalar);
                return std::string(std::same_as<T, i64> ? "i" : "f") + std::string(buffer, result.ptr);
            }
        },
        value
    );
}

void Hash(VariantKey& key) {
    std::string canonical;
    for (const auto& [name, value] : key.values)
        canonical += std::to_string(name.size()) + ":" + name + "=" + Encode(value) + ";";
    key.hash = Sha256(canonical);
}

} // namespace

VariantPlan PlanVariants(const ShaderDescriptor& descriptor, const u64 budget) {
    VariantPlan plan;
    u64 count = 1;
    for (const auto& domain : descriptor.permutations) {
        if (domain.values.empty() || count > budget / domain.values.size()) {
            plan.diagnostics.push_back({"SHD4001", DiagnosticSeverity::Error, "permutation cartesian product exceeds its budget", {}, {}});
            return plan;
        }
        count *= domain.values.size();
    }
    if (count > budget) {
        plan.diagnostics.push_back({"SHD4001", DiagnosticSeverity::Error, "permutation cartesian product exceeds its budget", {}, {}});
        return plan;
    }
    plan.variants.push_back({});
    for (const auto& domain : descriptor.permutations) {
        std::vector<VariantKey> expanded;
        expanded.reserve(plan.variants.size() * domain.values.size());
        for (const auto& partial : plan.variants)
            for (const auto& value : domain.values) {
                VariantKey next = partial;
                next.values.emplace_back(domain.name, value);
                expanded.push_back(std::move(next));
            }
        plan.variants = std::move(expanded);
    }
    for (auto& variant : plan.variants)
        Hash(variant);
    return plan;
}

Result<VariantKey> MakeVariantKey(const ShaderDescriptor& descriptor, const std::map<std::string, PermutationValue, std::less<>>& values) {
    if (values.size() != descriptor.permutations.size())
        return Err(ErrorCode::InvalidArgument, "variant does not assign every permutation");
    VariantKey key;
    for (const auto& domain : descriptor.permutations) {
        const auto selected = values.find(domain.name);
        if (selected == values.end() || std::ranges::find(domain.values, selected->second) == domain.values.end())
            return Err(ErrorCode::InvalidArgument, "variant assignment is outside its declared domain");
        key.values.emplace_back(domain.name, selected->second);
    }
    Hash(key);
    return Ok(std::move(key));
}

} // namespace woki::gfx
