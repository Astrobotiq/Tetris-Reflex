#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Application.hpp  —  Top-level engine entry point
//
// Application owns everything:
//   • The sf::RenderWindow (OS window + OpenGL context)
//   • The GameLoop (timing)
//   • The InputManager (keyboard / mouse state)
//   • The SceneManager (pushdown automaton of Scenes)
//
// WHY centralise ownership here?
//   Single clear owner means no ambiguity about lifetime. When Application is
//   destroyed, everything cleans up in the reverse order of construction —
//   RAII (Resource Acquisition Is Initialisation) in action.
//
// Usage:
//   engine::Application app("My Game", 1280, 720);
//   app.pushScene(std::make_unique<MyGameScene>());
//   app.run();
// ─────────────────────────────────────────────────────────────────────────────

#include "GameLoop.hpp"
#include "InputManager.hpp"
#include "SceneManager.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <memory>
#include <string>

namespace engine {

class Application {
public:
    Application(const std::string& title,
                unsigned int        width,
                unsigned int        height,
                unsigned int        targetFPS = 60);

    // Destructor is defaulted — RAII members clean themselves up.
    // We declare it to make the design intent explicit.
    ~Application() = default;

    // Non-copyable: an Application owns an OS window — copying makes no sense.
    // We explicitly delete copy/move to prevent accidental misuse.
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // Push the first scene and start the loop.
    void pushScene(std::unique_ptr<Scene> scene);
    void run();

    // Accessors for systems that need the window (e.g., coordinate conversion).
    sf::RenderWindow& window()       { return m_window; }
    InputManager&     input()        { return m_input; }
    SceneManager&     sceneManager() { return m_sceneManager; }

private:
    // Order matters for RAII: members are constructed top-to-bottom,
    // destroyed bottom-to-top. The window must exist before GameLoop.
    sf::RenderWindow m_window;
    InputManager     m_input;
    SceneManager     m_sceneManager;
    GameLoop         m_gameLoop;
};

} // namespace engine
