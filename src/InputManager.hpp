#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// InputManager.hpp  —  Keyboard & mouse state snapshot
//
// WHY NOT query sf::Keyboard::isKeyPressed() directly in game code?
//   1. It couples game logic tightly to SFML — hard to swap input backends.
//   2. It doesn't give you "pressed this frame" vs "held" distinction.
//   3. Centralising input lets you add rebinding, recording, replay later.
//
// We snapshot the full keyboard state once per frame in update(), then
// provide three queries:
//   isHeld(key)    — true every frame the key is down
//   isPressed(key) — true only on the FIRST frame the key goes down
//   isReleased(key)— true only on the frame the key comes up
// ─────────────────────────────────────────────────────────────────────────────

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/System/Vector2.hpp>
#include <array>

namespace engine {

class InputManager {
public:
    // Call once per logic tick (inside the fixed-timestep update).
    // Snapshots the entire keyboard and mouse state.
    void update();

    // ── Keyboard ─────────────────────────────────────────────────────────────

    // True every frame while the key is physically held down.
    bool isHeld(sf::Keyboard::Key key) const;

    // True on the single frame the key transitions from up → down.
    bool isPressed(sf::Keyboard::Key key) const;

    // True on the single frame the key transitions from down → up.
    bool isReleased(sf::Keyboard::Key key) const;

    // ── Mouse ─────────────────────────────────────────────────────────────────
    bool isMouseHeld(sf::Mouse::Button btn) const;
    bool isMousePressed(sf::Mouse::Button btn) const;
    sf::Vector2i getMousePosition() const;

private:
    // sf::Keyboard::KeyCount is the total number of keys SFML tracks.
    // We use std::array (fixed size, stack-allocated) rather than std::vector
    // because the size is known at compile time and never changes.
    static constexpr int KEY_COUNT = static_cast<int>(sf::Keyboard::KeyCount);
    static constexpr int BTN_COUNT = static_cast<int>(sf::Mouse::ButtonCount);

    std::array<bool, KEY_COUNT> m_currentKeys  {};
    std::array<bool, KEY_COUNT> m_previousKeys {};

    std::array<bool, BTN_COUNT> m_currentButtons  {};
    std::array<bool, BTN_COUNT> m_previousButtons {};

    sf::Vector2i m_mousePosition {};
};

} // namespace engine
