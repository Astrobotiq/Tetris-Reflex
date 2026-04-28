#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameLoop.hpp  —  Fixed-timestep update / variable-rate render loop
//
// THE PROBLEM with a naive loop (update + render every frame):
//   • Fast machines → physics runs too fast, game speed ties to FPS
//   • Slow machines → physics lags, inaccurate simulation
//
// THE SOLUTION: decouple physics/logic from rendering.
//   • Logic updates at a fixed timestep (e.g. 60 Hz) — deterministic
//   • Rendering runs as fast as the hardware allows
//   • A small "accumulator" carries leftover time between frames
//
// This is the "Fix Your Timestep" pattern (Gaffer on Games, 2004).
// ─────────────────────────────────────────────────────────────────────────────

#include <functional>
#include <SFML/System/Clock.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace engine {

class GameLoop {
public:
    // std::function lets callers pass lambdas, free functions, or member-function
    // wrappers — far more flexible than a raw function pointer.
    using EventFn = std::function<void(const sf::Event&)>;
    using UpdateFn = std::function<void(float dt)>;   // fixed dt in seconds
    using RenderFn = std::function<void(float alpha)>; // interpolation factor [0,1]

    // Default: 60 logic updates per second.
    // WHY 60? It matches common monitor refresh rates and gives stable physics.
    explicit GameLoop(sf::RenderWindow& window,
                      float             fixedTimestep = 1.f / 60.f);

    // Run until the window is closed.
    // Takes callbacks by const reference — no ownership transfer, no copy.
    void run(const EventFn& onEvent, const UpdateFn& onUpdate, const RenderFn& onRender);

    // ── Tuning ───────────────────────────────────────────────────────────────
    void  setFixedTimestep(float seconds) { m_fixedTimestep = seconds; }
    float getFixedTimestep() const        { return m_fixedTimestep; }

    // FPS of the render loop (last frame).
    float getFramesPerSecond() const { return m_fps; }

private:
    sf::RenderWindow& m_window;         // Non-owning reference; window lives in Application.
    float             m_fixedTimestep;  // Seconds between logic updates.
    float             m_fps = 0.f;
};

} // namespace engine
