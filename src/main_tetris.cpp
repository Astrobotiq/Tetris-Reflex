#include "GameLoop.hpp"
#include "InputManager.hpp"
#include "SceneManager.hpp"
#include "tetris/TetrisScene.hpp"
#include "tetris/MainMenuScene.hpp"
#include "tetris/AppSettings.hpp"
#include "WindowUtils.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <functional>

int main() {
    try {
        AppSettings settings;

        sf::RenderWindow window{sf::VideoMode({tetris::WIN_W, tetris::WIN_H}), "Tetris", sf::Style::Default, sf::State::Windowed};
        window.setFramerateLimit(60);

        updateViewport(window, tetris::WIN_W, tetris::WIN_H);

        engine::InputManager input;
        engine::GameLoop loop(window, 1.f / 60.f);

        engine::SceneManager sceneManager;

        // Uygulamayı Ana Menü ile başlatıyoruz
        sceneManager.push(std::make_unique<tetris::MainMenuScene>(window, &settings));

        loop.run(
            [&](const sf::Event &event) {
                if (event.is<sf::Event::Closed>()) {
                    window.close();
                }

                if (const auto* res = event.getIf<sf::Event::Resized>()) {
                    updateViewport(window, tetris::WIN_W, tetris::WIN_H);
                }

                if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::F11) {
                        settings.fullscreen = !settings.fullscreen;
                        if (settings.fullscreen) {
                            window.create(sf::VideoMode::getDesktopMode(), "Tetris", sf::Style::Default, sf::State::Fullscreen);
                        } else {
                            window.create(sf::VideoMode({tetris::WIN_W, tetris::WIN_H}), "Tetris", sf::Style::Default, sf::State::Windowed);
                        }
                        window.setFramerateLimit(60);
                        updateViewport(window, tetris::WIN_W, tetris::WIN_H);
                    }
                }

                sceneManager.handleEvent(event);
            },
            [&](float dt) {
                input.update();
                sceneManager.update(dt, input);
            },
            [&](float alpha) {
                sceneManager.render(window, alpha);
            }
        );

    } catch (const std::exception &e) {
        std::cerr << "[HATA] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}