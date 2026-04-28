#include "Application.hpp"

namespace engine {
    Application::Application(const std::string &title,
                             unsigned int width,
                             unsigned int height,
                             unsigned int targetFPS)
    // Member initialisation list: constructs members in declaration order.
    // This is preferred over assignment in the constructor body because:
    //   1. It directly initialises, not default-constructs + assigns.
    //   2. const and reference members MUST be initialised here.
        : m_window(sf::VideoMode({width, height}), title, sf::Style::Titlebar | sf::Style::Close)
          , m_input()
          , m_sceneManager()
          , m_gameLoop(m_window, 1.f / static_cast<float>(targetFPS)) {
        // Disable SFML's built-in vertical sync frame limit; our GameLoop manages timing.
        // You can enable VSync with m_window.setVerticalSyncEnabled(true) if preferred.
        m_window.setFramerateLimit(0);
    }

    void Application::pushScene(std::unique_ptr<Scene> scene) {
        m_sceneManager.push(std::move(scene));
    }

    void Application::run() {
        // Hand the GameLoop two lambdas:
        //   [&] captures 'this' by reference — safe because the lambdas don't
        //   outlive the Application object.
        m_gameLoop.run(
            [this](const sf::Event &event) { m_sceneManager.handleEvent(event); },
            [this](float dt) {
                m_input.update();
                m_sceneManager.update(dt, m_input);
            },
            [this](float alpha) { m_sceneManager.render(m_window, alpha); }
        );
    }
} // namespace engine
