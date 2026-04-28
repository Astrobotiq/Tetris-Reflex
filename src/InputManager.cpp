#include "InputManager.hpp"

namespace engine {

void InputManager::update() {
    // Shift current → previous BEFORE sampling new state.
    // This is the key trick for isPressed / isReleased detection:
    //   isPressed  = current && !previous
    //   isReleased = !current && previous
    m_previousKeys    = m_currentKeys;
    m_previousButtons = m_currentButtons;

    // Sample every key/button state in one pass.
    for (int i = 0; i < KEY_COUNT; ++i) {
        m_currentKeys[i] = sf::Keyboard::isKeyPressed(
            static_cast<sf::Keyboard::Key>(i));
    }

    for (int i = 0; i < BTN_COUNT; ++i) {
        m_currentButtons[i] = sf::Mouse::isButtonPressed(
            static_cast<sf::Mouse::Button>(i));
    }

    m_mousePosition = sf::Mouse::getPosition();
}

bool InputManager::isHeld(sf::Keyboard::Key key) const {
    return m_currentKeys[static_cast<int>(key)];
}

bool InputManager::isPressed(sf::Keyboard::Key key) const {
    int idx = static_cast<int>(key);
    return m_currentKeys[idx] && !m_previousKeys[idx];
}

bool InputManager::isReleased(sf::Keyboard::Key key) const {
    int idx = static_cast<int>(key);
    return !m_currentKeys[idx] && m_previousKeys[idx];
}

bool InputManager::isMouseHeld(sf::Mouse::Button btn) const {
    return m_currentButtons[static_cast<int>(btn)];
}

bool InputManager::isMousePressed(sf::Mouse::Button btn) const {
    int idx = static_cast<int>(btn);
    return m_currentButtons[idx] && !m_previousButtons[idx];
}

sf::Vector2i InputManager::getMousePosition() const {
    return m_mousePosition;
}

} // namespace engine
