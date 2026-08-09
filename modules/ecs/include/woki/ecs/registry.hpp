#pragma once

#include <span>
#include <tuple>
#include <limits>
#include <vector>
#include <cstddef>
#include <utility>
#include <concepts>
#include <iterator>
#include <stdexcept>
#include <typeindex>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include <woki/core.hpp>

#include "entity.hpp"

namespace woki {

template <bool IsConst, typename... Components>
class BasicView;

template <typename T>
concept Component = !std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_reference_v<T> && std::is_object_v<T> && std::destructible<T>;

namespace detail {

[[nodiscard]] constexpr u32 NextEntityGeneration(u32 generation) noexcept {
    const u32 next = generation + 1;
    return next == Entity::kInvalidGeneration ? 0u : next;
}

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    [[nodiscard]] virtual bool Remove(Entity entity) noexcept = 0;
    virtual void Clear() noexcept = 0;

    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;
    [[nodiscard]] virtual std::span<const Entity> DenseEntities() const noexcept = 0;
};

template <Component T>
class ComponentStorage final : public IComponentStorage {
public:
    static constexpr u32 kInvalidPosition = std::numeric_limits<u32>::max();

    template <typename... Args>
    T& Emplace(Entity entity, Args&&... args) {
        if (Contains(entity)) {
            throw std::logic_error("Entity already owns component");
        }

        const u32 index = entity.Index();
        EnsureSparse(index);

        auto component = createScope<T>(std::forward<Args>(args)...);
        const u32 position = static_cast<u32>(entities_.size());
        entities_.push_back(entity);
        try {
            data_.push_back(std::move(component));
        } catch (...) {
            entities_.pop_back();
            throw;
        }

        sparse_[index] = position;
        return *data_.back();
    }

    [[nodiscard]] bool Contains(Entity entity) const noexcept {
        const u32 index = entity.Index();
        if (index >= sparse_.size()) {
            return false;
        }

        const u32 position = sparse_[index];
        return position != kInvalidPosition && position < entities_.size() && entities_[position] == entity;
    }

    [[nodiscard]] T* TryGet(Entity entity) noexcept {
        if (!Contains(entity)) {
            return nullptr;
        }

        return data_[sparse_[entity.Index()]].get();
    }

    [[nodiscard]] const T* TryGet(Entity entity) const noexcept {
        if (!Contains(entity)) {
            return nullptr;
        }

        return data_[sparse_[entity.Index()]].get();
    }

    T& Get(Entity entity) {
        T* value = TryGet(entity);
        if (value == nullptr) {
            throw std::out_of_range("Entity does not own component");
        }
        return *value;
    }

    const T& Get(Entity entity) const {
        const T* value = TryGet(entity);
        if (value == nullptr) {
            throw std::out_of_range("Entity does not own component");
        }
        return *value;
    }

    [[nodiscard]] bool Remove(Entity entity) noexcept override {
        const u32 index = entity.Index();
        if (index >= sparse_.size()) {
            return false;
        }

        const u32 position = sparse_[index];
        if (position == kInvalidPosition || position >= entities_.size() || entities_[position] != entity) {
            return false;
        }

        const u32 last_position = static_cast<u32>(entities_.size() - 1);
        if (position != last_position) {
            entities_[position] = entities_[last_position];
            data_[position] = std::move(data_[last_position]);
            sparse_[entities_[position].Index()] = position;
        }

        entities_.pop_back();
        data_.pop_back();
        sparse_[index] = kInvalidPosition;
        return true;
    }

    void Clear() noexcept override {
        sparse_.clear();
        entities_.clear();
        data_.clear();
    }

    [[nodiscard]] std::size_t Size() const noexcept override {
        return entities_.size();
    }

    [[nodiscard]] std::span<const Entity> DenseEntities() const noexcept override {
        return entities_;
    }

private:
    void EnsureSparse(u32 index) {
        if (index >= sparse_.size()) {
            sparse_.resize(static_cast<std::size_t>(index) + 1, kInvalidPosition);
        }
    }

    std::vector<u32> sparse_;
    std::vector<Entity> entities_;
    std::vector<scope<T>> data_;
};

} // namespace detail

class Registry final {
public:
    static constexpr u32 kInvalidPosition = std::numeric_limits<u32>::max();

    Registry() = default;
    ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;

    [[nodiscard]] Entity Create() {
        if (!free_list_.empty()) {
            const u32 index = free_list_.back();
            const Entity entity(index, generations_[index]);
            entities_.push_back(entity);
            free_list_.pop_back();
            entity_positions_[index] = static_cast<u32>(entities_.size() - 1);
            return entity;
        }

        if (generations_.size() >= Entity::kInvalidIndex) {
            throw std::length_error("Entity index space exhausted");
        }

        const u32 index = static_cast<u32>(generations_.size());
        generations_.push_back(0);
        try {
            entity_positions_.push_back(kInvalidPosition);
        } catch (...) {
            generations_.pop_back();
            throw;
        }

        const Entity entity(index, 0);
        try {
            entities_.push_back(entity);
        } catch (...) {
            entity_positions_.pop_back();
            generations_.pop_back();
            throw;
        }

        entity_positions_[index] = static_cast<u32>(entities_.size() - 1);
        return entity;
    }

    [[nodiscard]] bool Destroy(Entity entity) {
        if (!Valid(entity)) {
            return false;
        }

        free_list_.reserve(free_list_.size() + 1);

        for (auto& [_, storage] : storages_) {
            (void)storage->Remove(entity);
        }

        const u32 index = entity.Index();
        const u32 position = entity_positions_[index];
        const u32 last_position = static_cast<u32>(entities_.size() - 1);
        if (position != last_position) {
            entities_[position] = entities_[last_position];
            entity_positions_[entities_[position].Index()] = position;
        }

        entities_.pop_back();
        entity_positions_[index] = kInvalidPosition;
        generations_[index] = detail::NextEntityGeneration(generations_[index]);
        free_list_.push_back(index);
        return true;
    }

    [[nodiscard]] bool Valid(Entity entity) const noexcept {
        if (!entity.Valid()) {
            return false;
        }

        const u32 index = entity.Index();
        return index < generations_.size() && entity_positions_[index] != kInvalidPosition && generations_[index] == entity.Generation();
    }

    void Clear() {
        free_list_.reserve(generations_.size());

        for (auto& [_, storage] : storages_) {
            storage->Clear();
        }

        entities_.clear();
        free_list_.clear();
        for (std::size_t position = 0; position < generations_.size(); ++position) {
            const u32 index = static_cast<u32>(position);
            generations_[index] = detail::NextEntityGeneration(generations_[index]);
            entity_positions_[index] = kInvalidPosition;
            free_list_.push_back(index);
        }
    }

    [[nodiscard]] std::size_t Size() const noexcept {
        return entities_.size();
    }

    [[nodiscard]] bool Empty() const noexcept {
        return entities_.empty();
    }

    [[nodiscard]] std::span<const Entity> Entities() const noexcept {
        return entities_;
    }

    [[nodiscard]] std::size_t Capacity() const noexcept {
        return generations_.size();
    }

    template <typename Func>
    void EachEntity(Func&& func) const {
        for (Entity entity : entities_) {
            std::invoke(func, entity);
        }
    }

    template <Component T, typename... Args>
    T& Emplace(Entity entity, Args&&... args) {
        if (!Valid(entity)) {
            throw std::invalid_argument("Cannot add component to dead entity");
        }
        return EnsureStorage<T>().Emplace(entity, std::forward<Args>(args)...);
    }

    template <Component T, typename... Args>
    T& GetOrEmplace(Entity entity, Args&&... args) {
        if (!Valid(entity)) {
            throw std::invalid_argument("Cannot access component on dead entity");
        }

        if (T* value = TryGet<T>(entity)) {
            return *value;
        }

        return EnsureStorage<T>().Emplace(entity, std::forward<Args>(args)...);
    }

    template <Component T>
    [[nodiscard]] bool Has(Entity entity) const noexcept {
        if (!Valid(entity)) {
            return false;
        }

        const auto* storage = FindStorage<T>();
        return storage != nullptr && storage->Contains(entity);
    }

    template <Component... Components>
    [[nodiscard]] bool AllOf(Entity entity) const noexcept {
        return (Has<Components>(entity) && ...);
    }

    template <Component... Components>
    [[nodiscard]] bool AnyOf(Entity entity) const noexcept {
        return (Has<Components>(entity) || ...);
    }

    template <Component T>
    [[nodiscard]] T* TryGet(Entity entity) noexcept {
        if (!Valid(entity)) {
            return nullptr;
        }

        auto* storage = FindStorage<T>();
        return storage != nullptr ? storage->TryGet(entity) : nullptr;
    }

    template <Component T>
    [[nodiscard]] const T* TryGet(Entity entity) const noexcept {
        if (!Valid(entity)) {
            return nullptr;
        }

        const auto* storage = FindStorage<T>();
        return storage != nullptr ? storage->TryGet(entity) : nullptr;
    }

    template <Component T>
    T& Get(Entity entity) {
        T* value = TryGet<T>(entity);
        if (value == nullptr) {
            throw std::out_of_range("Entity does not own component");
        }
        return *value;
    }

    template <Component T>
    const T& Get(Entity entity) const {
        const T* value = TryGet<T>(entity);
        if (value == nullptr) {
            throw std::out_of_range("Entity does not own component");
        }
        return *value;
    }

    template <Component T>
    [[nodiscard]] bool Remove(Entity entity) noexcept {
        auto* storage = FindStorage<T>();
        return storage != nullptr && storage->Remove(entity);
    }

    template <Component T>
    [[nodiscard]] std::size_t Count() const noexcept {
        const auto* storage = FindStorage<T>();
        return storage != nullptr ? storage->Size() : 0u;
    }

    template <Component T>
    void Clear() noexcept {
        auto* storage = FindStorage<T>();
        if (storage == nullptr) {
            return;
        }

        storage->Clear();
    }

    template <Component... Components>
    [[nodiscard]] auto View() & -> BasicView<false, Components...>;

    template <Component... Components>
    [[nodiscard]] auto View() const& -> BasicView<true, Components...>;

private:
    template <bool IsConst, typename... Components>
    friend class BasicView;

    template <Component T>
    [[nodiscard]] detail::ComponentStorage<T>* FindStorage() noexcept {
        const auto it = storages_.find(std::type_index(typeid(T)));
        if (it == storages_.end()) {
            return nullptr;
        }

        return static_cast<detail::ComponentStorage<T>*>(it->second.get());
    }

    template <Component T>
    [[nodiscard]] const detail::ComponentStorage<T>* FindStorage() const noexcept {
        const auto it = storages_.find(std::type_index(typeid(T)));
        if (it == storages_.end()) {
            return nullptr;
        }

        return static_cast<const detail::ComponentStorage<T>*>(it->second.get());
    }

    template <Component T>
    detail::ComponentStorage<T>& EnsureStorage() {
        const auto key = std::type_index(typeid(T));
        const auto it = storages_.find(key);
        if (it != storages_.end()) {
            return *static_cast<detail::ComponentStorage<T>*>(it->second.get());
        }

        auto storage = createScope<detail::ComponentStorage<T>>();
        auto* storage_ptr = storage.get();
        storages_.emplace(key, std::move(storage));
        return *storage_ptr;
    }

    std::vector<u32> generations_;
    std::vector<u32> entity_positions_;
    std::vector<Entity> entities_;
    std::vector<u32> free_list_;
    std::unordered_map<std::type_index, scope<detail::IComponentStorage>> storages_;
};

template <bool IsConst, typename... Components>
class BasicView final {
public:
    static_assert((Component<Components> && ...), "Views only support component types");
    static_assert(sizeof...(Components) > 0, "Views require at least one component type");

    using RegistryType = std::conditional_t<IsConst, const Registry, Registry>;

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entity;
        using difference_type = std::ptrdiff_t;

        Iterator() = default;

        explicit Iterator(const BasicView* view, std::size_t index) noexcept
            : view_(view),
              index_(index) {
            Advance();
        }

        [[nodiscard]] Entity operator*() const noexcept {
            return view_->entities_[index_];
        }

        Iterator& operator++() noexcept {
            ++index_;
            Advance();
            return *this;
        }

        Iterator operator++(int) noexcept {
            Iterator previous = *this;
            ++(*this);
            return previous;
        }

        [[nodiscard]] bool operator==(const Iterator& other) const noexcept = default;

    private:
        void Advance() noexcept {
            while (view_ != nullptr && index_ < view_->entities_.size() && !view_->ContainsAll(view_->entities_[index_])) {
                ++index_;
            }
        }

        const BasicView* view_ = nullptr;
        std::size_t index_ = 0;
    };

    explicit BasicView(RegistryType& registry)
        : registry_(&registry) {
        SelectLeadStorage();
    }

    [[nodiscard]] Iterator begin() const noexcept {
        return Iterator(this, 0);
    }

    [[nodiscard]] Iterator end() const noexcept {
        return Iterator(this, entities_.size());
    }

    [[nodiscard]] bool Empty() const noexcept {
        return begin() == end();
    }

    [[nodiscard]] std::size_t Size() const noexcept {
        std::size_t count = 0;
        for ([[maybe_unused]] Entity entity : *this) {
            ++count;
        }
        return count;
    }

    [[nodiscard]] auto Get(Entity entity) const {
        if (!ContainsAll(entity)) {
            throw std::out_of_range("Entity is not part of this view");
        }
        return std::forward_as_tuple(registry_->template Get<Components>(entity)...);
    }

    template <typename Func>
    void Each(Func&& func) const {
        for (Entity entity : *this) {
            if constexpr (std::is_invocable_v<Func, Entity, decltype(registry_->template Get<Components>(entity))...>) {
                std::invoke(func, entity, registry_->template Get<Components>(entity)...);
            } else {
                std::invoke(func, registry_->template Get<Components>(entity)...);
            }
        }
    }

private:
    template <Component T>
    [[nodiscard]] const detail::IComponentStorage* CandidateStorage() const noexcept {
        return registry_->template FindStorage<T>();
    }

    void SelectLeadStorage() {
        const detail::IComponentStorage* lead = nullptr;

        auto select_candidate = [&](const detail::IComponentStorage* candidate) {
            if (candidate == nullptr) {
                lead = nullptr;
                return false;
            }

            if (lead == nullptr || candidate->Size() < lead->Size()) {
                lead = candidate;
            }

            return true;
        };

        if (!(select_candidate(CandidateStorage<Components>()) && ...)) {
            entities_ = {};
            return;
        }

        if (lead == nullptr) {
            entities_.clear();
            return;
        }

        const auto entities = lead->DenseEntities();
        entities_.assign(entities.begin(), entities.end());
    }

    [[nodiscard]] bool ContainsAll(Entity entity) const noexcept {
        return registry_->template AllOf<Components...>(entity);
    }

    RegistryType* registry_ = nullptr;
    std::vector<Entity> entities_{};
};

template <Component... Components>
[[nodiscard]] inline auto Registry::View() & -> BasicView<false, Components...> {
    return BasicView<false, Components...>(*this);
}

template <Component... Components>
[[nodiscard]] inline auto Registry::View() const& -> BasicView<true, Components...> {
    return BasicView<true, Components...>(*this);
}

} // namespace woki
