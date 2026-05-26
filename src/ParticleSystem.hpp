#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// systems/ParticleSystem.hpp  —  Object-pool based particle emitter
//
// DESIGN GOALS:
//   • Zero heap allocation after startup (object pool).
//   • Supports both BURST (explosion) and CONTINUOUS (fire, smoke) emitters.
//   • Particles rendered as a single sf::VertexArray draw call (Points or Quads).
//   • Each particle has: position, velocity, colour, lifetime, size.
//   • Colour and size interpolate from "birth" values to "death" values.
//
// THE OBJECT POOL PATTERN:
//   Instead of new/delete per particle (expensive, causes fragmentation),
//   we allocate a fixed array of Particle structs upfront. "Dead" particles
//   sit in the pool waiting to be recycled. This is cache-friendly and
//   produces zero garbage for the allocator.
//
// EMITTER vs PARTICLE:
//   Emitter — configuration (spawn rate, spread, colours…). Attached to entities.
//   Particle — runtime data (current pos, vel, remaining life…). Managed by system.
// ─────────────────────────────────────────────────────────────────────────────

#include "../ECS.hpp"
#include "../Components.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <cmath>
#include <random>

namespace engine {

// ─────────────────────────────────────────────────────────────────────────────
// Particle  —  Internal runtime state (not an ECS component; owned by the pool)
// ─────────────────────────────────────────────────────────────────────────────
struct Particle {
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color    colorStart;
    sf::Color    colorEnd;
    float        sizeStart    { 4.f };
    float        sizeEnd      { 0.f };
    float        lifetime     { 1.f }; // Total lifetime in seconds
    float        age          { 0.f }; // Current age in seconds
    bool         alive        { false };

    float normalizedAge() const {
        return (lifetime > 0.f) ? (age / lifetime) : 1.f;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// EmitterComponent  —  Configuration attached to an entity as an ECS component
// ─────────────────────────────────────────────────────────────────────────────
struct EmitterComponent {
    // Spawn settings
    float  emissionRate    { 50.f };   // Particles per second (for continuous)
    int    burstCount      { 0 };      // Particles per burst (0 = continuous)
    bool   isBurst         { false };  // If true, emit burstCount once then stop.
    bool   active          { true };

    // Particle initial properties (each value ± spread for randomness)
    float  speed           { 100.f };
    float  speedSpread     { 40.f  };
    float  direction       { -90.f }; // Degrees; -90 = upward
    float  directionSpread { 45.f  }; // Half-angle cone, degrees
    float  lifetime        { 1.2f  };
    float  lifetimeSpread  { 0.4f  };
    float  sizeStart       { 6.f   };
    float  sizeEnd         { 0.f   };
    sf::Color colorStart   { 255, 180, 60, 255 };
    sf::Color colorEnd     { 255,  60,  0,   0 }; // Alpha → 0 = fade out

    // Gravity scale (1.0 = full gravity, 0.0 = weightless particles)
    float gravityScale     { 0.3f };

    // Internal accumulator for continuous emission.
    float _accumulator     { 0.f };
    bool  _burstFired      { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// ParticleSystem
// ─────────────────────────────────────────────────────────────────────────────

// Pool size: maximum live particles across ALL emitters.
// Tune this for your game. Larger = more RAM, smaller = visible pop-in.
inline constexpr std::size_t MAX_PARTICLES = 4096;

class ParticleSystem {
public:
    explicit ParticleSystem(Registry& registry)
        : m_registry(registry)
    {
        // Seed the random engine. std::mt19937 is a high-quality, fast PRNG.
        // We use it instead of rand() because rand() has poor statistical
        // properties and is not thread-safe.
        std::random_device rd;
        m_rng.seed(rd());

        m_signature = registry.buildSignature<TransformComponent, EmitterComponent>();
    }

    // World gravity (should match PhysicsSystem::gravity for consistency).
    sf::Vector2f gravity { 0.f, 980.f };

    void update(float dt) {
        // ── 1. Spawn new particles from emitters ──────────────────────────────
        for (Entity e : m_registry.view(m_signature)) {
            auto& tf  = m_registry.getComponent<TransformComponent>(e);
            auto& em  = m_registry.getComponent<EmitterComponent>(e);

            if (!em.active) continue;

            if (em.isBurst) {
                if (!em._burstFired) {
                    for (int i = 0; i < em.burstCount; ++i) {
                        spawnParticle(tf.position, em);
                    }
                    em._burstFired = true;
                    em.active = false; // Burst emitters auto-deactivate.
                }
            } else {
                // Continuous: accumulate fractional particles over time.
                em._accumulator += em.emissionRate * dt;
                while (em._accumulator >= 1.f) {
                    spawnParticle(tf.position, em);
                    em._accumulator -= 1.f;
                }
            }
        }

        // ── 2. Update live particles ──────────────────────────────────────────
        for (auto& p : m_pool) {
            if (!p.alive) continue;

            p.age += dt;
            if (p.age >= p.lifetime) {
                p.alive = false;
                --m_aliveCount;
                continue;
            }

            // Apply gravity (scaled by emitter's gravityScale — stored in particle
            // via a workaround: we bake it into velocity at spawn time instead,
            // keeping the per-particle struct minimal).
            // Here we apply a fixed gravity fraction stored per-particle.
            // (We store gravityScale in a separate parallel array for cache reasons.)
            p.velocity += gravity * m_gravityScales[&p - m_pool.data()] * dt;
            p.position += p.velocity * dt;
        }
    }

    void render(sf::RenderWindow& window) {
        // Build a VertexArray of Triangles — 6 vertices per particle (2 triangles for a quad).
        m_vertices.clear();
        m_vertices.setPrimitiveType(sf::PrimitiveType::Triangles);

        for (const auto& p : m_pool) {
            if (!p.alive) continue;

            float t   = p.normalizedAge();
            float inv = 1.f - t;

            // Lerp colour between birth and death values.
            sf::Color c(
                static_cast<std::uint8_t>(p.colorStart.r * inv + p.colorEnd.r * t),
                static_cast<std::uint8_t>(p.colorStart.g * inv + p.colorEnd.g * t),
                static_cast<std::uint8_t>(p.colorStart.b * inv + p.colorEnd.b * t),
                static_cast<std::uint8_t>(p.colorStart.a * inv + p.colorEnd.a * t)
            );

            float half = (p.sizeStart * inv + p.sizeEnd * t) * 0.5f;
            sf::Vector2f pos = p.position;

            sf::Vector2f topLeft{pos.x - half, pos.y - half};
            sf::Vector2f topRight{pos.x + half, pos.y - half};
            sf::Vector2f bottomRight{pos.x + half, pos.y + half};
            sf::Vector2f bottomLeft{pos.x - half, pos.y + half};

            // Triangle 1: Top-Left, Top-Right, Bottom-Right
            m_vertices.append(sf::Vertex{topLeft, c});
            m_vertices.append(sf::Vertex{topRight, c});
            m_vertices.append(sf::Vertex{bottomRight, c});

            // Triangle 2: Top-Left, Bottom-Right, Bottom-Left
            m_vertices.append(sf::Vertex{topLeft, c});
            m_vertices.append(sf::Vertex{bottomRight, c});
            m_vertices.append(sf::Vertex{bottomLeft, c});
        }

        window.draw(m_vertices);
    }

    // Manually spawn a burst at a world position (e.g., on enemy death).
    void burst(sf::Vector2f position, const EmitterComponent& config, int count) {
        for (int i = 0; i < count; ++i) {
            spawnParticle(position, config);
        }
    }

    std::size_t aliveCount() const { return m_aliveCount; }

private:
    void spawnParticle(sf::Vector2f pos, const EmitterComponent& em) {
        // Find a dead slot in the pool (linear scan; fast in practice because
        // recently-dead particles are near the front of the array).
        for (std::size_t i = 0; i < MAX_PARTICLES; ++i) {
            Particle& p = m_pool[i];
            if (p.alive) continue;

            // ── Randomise initial values ──────────────────────────────────────
            // std::uniform_real_distribution gives values in [min, max].
            // We capture it by reference in a lambda for brevity.
            auto randf = [&](float min, float max) -> float {
                return std::uniform_real_distribution<float>{min, max}(m_rng);
            };

            float angle = (em.direction + randf(-em.directionSpread, em.directionSpread))
                          * (3.14159265f / 180.f);
            float speed = em.speed + randf(-em.speedSpread, em.speedSpread);

            p.position   = pos;
            p.velocity   = { std::cos(angle) * speed, std::sin(angle) * speed };
            p.colorStart = em.colorStart;
            p.colorEnd   = em.colorEnd;
            p.sizeStart  = em.sizeStart;
            p.sizeEnd    = em.sizeEnd;
            p.lifetime   = std::max(0.01f, em.lifetime + randf(-em.lifetimeSpread, em.lifetimeSpread));
            p.age        = 0.f;
            p.alive      = true;

            m_gravityScales[i] = em.gravityScale;
            ++m_aliveCount;
            return;
        }
        // Pool exhausted — silently drop the particle. In a real engine you'd
        // log a warning or increase MAX_PARTICLES.
    }

    Registry& m_registry;
    Signature m_signature;

    // The pool: fixed-size array, zero heap allocation after construction.
    std::array<Particle, MAX_PARTICLES> m_pool     {};
    std::array<float,    MAX_PARTICLES> m_gravityScales {};
    std::size_t                         m_aliveCount { 0 };

    sf::VertexArray m_vertices;
    std::mt19937    m_rng; // Mersenne Twister PRNG
};

} // namespace engine
