#include <array>

#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

using namespace woki;
using namespace woki::asset;

TEST_CASE("VFS selects mount priority and supports exact partial reads") {
    const auto path = AssetPath::Parse("data/file.bin");
    const auto uri = AssetUri::Parse("project://data/file.bin");
    REQUIRE(path);
    REQUIRE(uri);
    const auto low = createRef<MemoryMount>();
    const auto high = createRef<MemoryMount>();
    low->PutText(*path, "low");
    high->PutText(*path, "0123456789");
    Vfs vfs;
    REQUIRE(vfs.MountAt("low", AssetScheme::Project, {}, 1, low));
    REQUIRE(vfs.MountAt("high", AssetScheme::Project, {}, 10, high));
    REQUIRE_FALSE(vfs.MountAt("high", AssetScheme::Project, {}, 20, high));
    REQUIRE(vfs.ReadText(*uri) == "0123456789");
    const auto partial = vfs.ReadBinary(*uri, 3, 4);
    REQUIRE(partial);
    REQUIRE(std::to_integer<char>((*partial)[0]) == '3');
    REQUIRE(std::to_integer<char>((*partial)[3]) == '6');
    REQUIRE(vfs.ReadBinary(*uri, 8, 4)->size() == 2);
    REQUIRE(vfs.Enumerate(AssetScheme::Project)->size() == 1);
    vfs.Unmount("high");
    REQUIRE(vfs.ReadText(*uri) == "low");
}

TEST_CASE("AssetDatabase transactions are atomic conflict-aware and round trip") {
    AssetDatabase database;
    const auto id = AssetId::FromName("database-a");
    const auto uri = AssetUri::Parse("project://a.asset");
    REQUIRE(uri);
    AssetRecord record{id, *uri, 1, 2, Sha256("source"), Sha256("product"), 3, 4};

    auto first = database.BeginTransaction();
    auto stale = database.BeginTransaction();
    REQUIRE(first.Upsert(record));
    REQUIRE(first.Commit());
    REQUIRE_FALSE(first.Commit());
    REQUIRE_FALSE(stale.Commit());
    REQUIRE(database.Find(id)->revision == 3);
    REQUIRE(database.Find(*uri)->id == id);

    auto duplicate = database.BeginTransaction();
    auto other = record;
    other.id = AssetId::FromName("database-b");
    REQUIRE_FALSE(duplicate.Upsert(other));

    const auto serialized = database.Serialize();
    REQUIRE(serialized);
    AssetDatabase restored;
    REQUIRE(restored.ParseAndReplace(*serialized));
    REQUIRE(restored.Records().size() == 1);
    auto trailing = *serialized;
    trailing.push_back(std::byte{});
    REQUIRE_FALSE(restored.ParseAndReplace(trailing));
    REQUIRE(restored.Records().size() == 1);
    REQUIRE_FALSE(database.Serialize({.max_records = 0}));
}

TEST_CASE("DependencyGraph rejects cycles without mutating its previous graph") {
    DependencyGraph graph;
    const auto a = AssetId::FromName("graph-a");
    const auto b = AssetId::FromName("graph-b");
    const auto c = AssetId::FromName("graph-c");
    const ProductDependency on_b{b, Sha256("b")};
    const ProductDependency on_c{c, Sha256("c")};
    REQUIRE(graph.Replace(a, std::array{on_b}));
    REQUIRE(graph.Replace(b, std::array{on_c}));
    REQUIRE(graph.DirectDependents(c) == std::vector<AssetId>{b});
    const auto transitive = graph.TransitiveDependents(c);
    REQUIRE(transitive.size() == 2);
    REQUIRE(std::ranges::find(transitive, a) != transitive.end());
    REQUIRE(std::ranges::find(transitive, b) != transitive.end());
    const ProductDependency on_a{a, Sha256("a")};
    REQUIRE_FALSE(graph.Replace(c, std::array{on_a}));
    REQUIRE(graph.Dependencies(c).empty());
    REQUIRE(graph.ValidateAcyclic());
    REQUIRE_FALSE(graph.Replace(a, std::array{on_b, on_b}));
    graph.Remove(b);
    REQUIRE(graph.DirectDependents(c).empty());
}
