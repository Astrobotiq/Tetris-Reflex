#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ECS.hpp  —  Entity-Component-System core
//
// DESIGN PHILOSOPHY:
//   An ECS separates DATA (Components) from IDENTITY (Entities) and LOGIC
//   (Systems). This gives us:
//     • Cache-friendly memory layout (components stored in contiguous arrays)
//     • Easy composition — add/remove behaviours without inheritance trees
//     • Clear separation of concerns
//
// ARCHITECTURE OVERVIEW:
//   Entity    — just a numeric ID (uint32_t). Nothing more.
//   Component — plain data structs (Transform, Sprite, etc.)
//   Registry  — owns all components; the "world" or "database"
//   System    — free functions / classes that query the Registry and act on data
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <limits>

namespace engine {

// ─────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────

// The maximum number of live entities. Tune this for your game.
// Using a compile-time constant lets us use fixed-size arrays (no heap alloc).
inline constexpr std::size_t MAX_ENTITIES   = 5000;

// The maximum number of distinct component types.
// 32 is plenty for a simple engine; bump to 64 if you need more.
inline constexpr std::size_t MAX_COMPONENTS = 32;

// ─────────────────────────────────────────────────────────────────────────────
// TYPES
// ─────────────────────────────────────────────────────────────────────────────

// Entity is just an ID. Using a typedef makes intent crystal-clear in signatures.
using Entity          = std::uint32_t;

// A Signature is a bitmask: bit N is set if the entity has component type N.
// This lets us quickly check "does entity X have components A, B, and C?"
// with a single bitwise AND.
using Signature       = std::bitset<MAX_COMPONENTS>;

// Each component type gets a unique integer ID at runtime.
using ComponentTypeID = std::uint8_t;

inline constexpr Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();

// ─────────────────────────────────────────────────────────────────────────────
// COMPONENT TYPE ID GENERATOR
// A static local variable inside a template function is initialised exactly
// once per instantiation — giving each component type a unique, stable ID.
// ─────────────────────────────────────────────────────────────────────────────
inline ComponentTypeID nextComponentTypeID() {
    static ComponentTypeID id = 0;
    assert(id < MAX_COMPONENTS && "Too many component types registered!");
    return id++;
}

template<typename T>
ComponentTypeID componentTypeID() {
    // 'static' here means this variable is initialised once for the entire
    // program lifetime, not once per call. The lambda is called exactly once.
    static ComponentTypeID id = nextComponentTypeID();
    return id;
}

// ─────────────────────────────────────────────────────────────────────────────
// COMPONENT ARRAY  (sparse-set / dense array hybrid)
//
// WHY NOT std::vector<T> indexed by Entity?
//   That wastes memory when most entities don't have component T.
//
// THIS APPROACH: two parallel arrays + a map
//   • m_dense   — tightly packed array of components (cache-friendly iteration)
//   • m_entities — which entity owns each dense slot
//   • m_sparse  — maps Entity → dense index (fast O(1) lookup)
// ─────────────────────────────────────────────────────────────────────────────

// Abstract base so we can store heterogeneous component arrays in one container.
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void onEntityDestroyed(Entity entity) = 0;
};

template<typename T>
class ComponentArray : public IComponentArray {
public:
    void insert(Entity entity, T component) {
        assert(m_entityToIndex.find(entity) == m_entityToIndex.end()
               && "Component added to same entity twice.");

        std::size_t newIndex = m_size;
        m_entityToIndex[entity]    = newIndex;
        m_indexToEntity[newIndex]  = entity;
        m_componentArray[newIndex] = std::move(component);
        ++m_size;
    }

    void remove(Entity entity) {
        assert(m_entityToIndex.count(entity) && "Removing non-existent component.");

        // Swap the removed component with the last one to keep the array packed.
        // This is the key insight of the sparse-set: O(1) removal without gaps.
        std::size_t removedIndex     = m_entityToIndex[entity];
        std::size_t lastIndex        = m_size - 1;
        m_componentArray[removedIndex] = m_componentArray[lastIndex];

        // Update the maps to reflect the swap.
        Entity lastEntity = m_indexToEntity[lastIndex];
        m_entityToIndex[lastEntity]  = removedIndex;
        m_indexToEntity[removedIndex] = lastEntity;

        m_entityToIndex.erase(entity);
        m_indexToEntity.erase(lastIndex);
        --m_size;
    }

    // Returns a reference — the caller can modify the component in-place.
    // We use assert() to catch programming errors in debug builds; in release
    // builds asserts are compiled out for zero overhead.
    T& get(Entity entity) {
        assert(m_entityToIndex.count(entity) && "Retrieving non-existent component.");
        return m_componentArray[m_entityToIndex[entity]];
    }

    bool has(Entity entity) const {
        return m_entityToIndex.count(entity) > 0;
    }

    void onEntityDestroyed(Entity entity) override {
        if (m_entityToIndex.count(entity)) {
            remove(entity);
        }
    }

private:
    // std::array has a fixed compile-time size — no heap reallocation ever.
    std::array<T, MAX_ENTITIES>           m_componentArray{};
    std::unordered_map<Entity, std::size_t> m_entityToIndex{};
    std::unordered_map<std::size_t, Entity> m_indexToEntity{};
    std::size_t                             m_size = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// ENTITY MANAGER
// Hands out Entity IDs and tracks which signatures each entity has.
// ─────────────────────────────────────────────────────────────────────────────
class EntityManager {
public:
    EntityManager() {
        // Pre-fill the queue with all available IDs.
        // Using a queue gives us FIFO recycling — recently-destroyed IDs are
        // reused last, reducing accidental stale-reference bugs.
        for (Entity e = 0; e < MAX_ENTITIES; ++e) {
            m_availableEntities.push(e);
        }
    }

    Entity createEntity() {
        assert(!m_availableEntities.empty() && "Too many entities!");
        Entity id = m_availableEntities.front();
        m_availableEntities.pop();
        ++m_livingEntityCount;
        return id;
    }

    void destroyEntity(Entity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        m_signatures[entity].reset(); // Clear all component bits.
        m_availableEntities.push(entity);
        --m_livingEntityCount;
    }

    void setSignature(Entity entity, Signature sig) {
        assert(entity < MAX_ENTITIES);
        m_signatures[entity] = sig;
    }

    Signature getSignature(Entity entity) const {
        assert(entity < MAX_ENTITIES);
        return m_signatures[entity];
    }

    std::uint32_t livingCount() const { return m_livingEntityCount; }

private:
    std::queue<Entity>                 m_availableEntities{};
    std::array<Signature, MAX_ENTITIES> m_signatures{};
    std::uint32_t                       m_livingEntityCount = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// REGISTRY  (the "World")
//
// The Registry is the central hub. It owns the EntityManager and all
// ComponentArrays. Game code goes through the Registry for everything ECS.
//
// We use std::shared_ptr here because Systems hold a reference to the Registry.
// An alternative design stores the Registry as a global singleton, but
// shared_ptr ownership is safer and more testable.
// ─────────────────────────────────────────────────────────────────────────────
class Registry {
public:
    // ── Entity lifecycle ────────────────────────────────────────────────────

    Entity createEntity() {
        return m_entityManager.createEntity();
    }

    void destroyEntity(Entity entity) {
        m_entityManager.destroyEntity(entity);
        // Notify every component array to remove this entity's data.
        for (auto& [typeIdx, array] : m_componentArrays) {
            array->onEntityDestroyed(entity);
        }
    }

    // ── Component registration & access ────────────────────────────────────

    // Call this once per component type at startup.
    // We use a template so the type system enforces correctness at compile time.
    template<typename T>
    void registerComponent() {
        std::type_index ti(typeid(T));
        assert(!m_componentArrays.count(ti) && "Registering component type more than once.");
        m_componentArrays[ti] = std::make_shared<ComponentArray<T>>();
    }

    template<typename T>
    void addComponent(Entity entity, T component) {
        getComponentArray<T>()->insert(entity, std::move(component));

        // Update the entity's signature bitmask.
        auto sig = m_entityManager.getSignature(entity);
        sig.set(componentTypeID<T>());
        m_entityManager.setSignature(entity, sig);
    }

    template<typename T>
    void removeComponent(Entity entity) {
        getComponentArray<T>()->remove(entity);

        auto sig = m_entityManager.getSignature(entity);
        sig.reset(componentTypeID<T>());
        m_entityManager.setSignature(entity, sig);
    }

    // Returns a reference so callers can modify the component in-place:
    //   registry.getComponent<Transform>(e).position.x += 5.f;
    template<typename T>
    T& getComponent(Entity entity) {
        return getComponentArray<T>()->get(entity);
    }

    template<typename T>
    bool hasComponent(Entity entity) const {
        auto it = m_componentArrays.find(std::type_index(typeid(T)));
        if (it == m_componentArrays.end()) return false;
        return std::static_pointer_cast<ComponentArray<T>>(it->second)->has(entity);
    }

    template<typename T>
    ComponentTypeID getComponentTypeID() const {
        return componentTypeID<T>();
    }

    Signature getSignature(Entity entity) const {
        return m_entityManager.getSignature(entity);
    }

    // ── Convenience: iterate entities that match a signature ───────────────
    // Returns all entities that have AT LEAST the requested components.
    // Signature is built with registry.buildSignature<A,B,C>().
    std::vector<Entity> view(Signature requiredSig) const {
        std::vector<Entity> result;
        result.reserve(64);
        for (Entity e = 0; e < MAX_ENTITIES; ++e) {
            // bitwise AND: all required bits must be set
            if ((m_entityManager.getSignature(e) & requiredSig) == requiredSig) {
                result.push_back(e);
            }
        }
        return result;
    }

    // Build a Signature from a variadic list of component types.
    // Variadic templates + fold expressions (C++17) let us write:
    //   buildSignature<Transform, SpriteComponent>()
    template<typename... Ts>
    Signature buildSignature() {
        Signature sig;
        // C++17 fold expression: expands to sig.set(id<T1>()), sig.set(id<T2>()), ...
        (sig.set(componentTypeID<Ts>()), ...);
        return sig;
    }

private:
    template<typename T>
    std::shared_ptr<ComponentArray<T>> getComponentArray() {
        std::type_index ti(typeid(T));
        assert(m_componentArrays.count(ti) && "Component not registered.");
        // static_pointer_cast: safe downcast since we know the type.
        return std::static_pointer_cast<ComponentArray<T>>(m_componentArrays[ti]);
    }

    EntityManager m_entityManager;

    // We store component arrays by type_index so we can look them up by type.
    // shared_ptr because IComponentArray is polymorphic (virtual destructor).
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_componentArrays;
};

} // namespace engine
