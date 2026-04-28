#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// systems/RenderSystem.hpp  —  Queries ECS for renderable entities and draws them
//
// A System is NOT a class that inherits from a base System class in our design.
// It is simply code that knows which components to query and what to do with them.
// We use a plain class with a single render() method.
//
// The RenderSystem looks for entities that have BOTH:
//   • TransformComponent — where to draw
//   • SpriteComponent    — what to draw
// It then syncs the sf::Sprite's transform from TransformComponent and draws it.
// ─────────────────────────────────────────────────────────────────────────────

#include "ECS.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

namespace engine {

class RenderSystem {
public:
    // Takes a non-owning reference to the Registry so it can query entities.
    // The System does NOT own the Registry — the Scene does.
    explicit RenderSystem(Registry& registry);

    // Called each render frame with the interpolation alpha from GameLoop.
    // alpha ∈ [0,1) — for now we ignore it, but leave it here for sub-step
    // interpolation when you add a previous-position component later.
    void render(sf::RenderWindow& window, float alpha);

private:
    Registry& m_registry;  // Non-owning reference — no lifetime issues here
                            // because the System is owned by the Scene which
                            // also owns the Registry.
    Signature m_signature; // Cached: entities with Transform + Sprite
};

} // namespace engine
