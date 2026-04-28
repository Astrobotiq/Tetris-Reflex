#include "GameLoop.hpp"
#include "InputManager.hpp"
#include "tetris/TetrisScene.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>

int main() {
    try {
        // GERÇEK PENCERE BURADA 1 KERE YARATILIYOR
        sf::RenderWindow window{sf::VideoMode({tetris::WIN_W, tetris::WIN_H}), "Tetris"};
        window.setFramerateLimit(60); // Bunu 60 yapmak CPU'nu rahatlatır (0 ise CPU %100 çalışır)

        engine::InputManager input;
        engine::GameLoop loop(window, 1.f / 60.f);

        // TetrisScene engine Scene'den türüyor ama burada doğrudan kullanıyoruz.
        tetris::TetrisScene scene(window);
        scene.onEnter();

        loop.run(
            // 1. Olay (Event) Callback
            [&](const sf::Event &event) {
                scene.handleEvent(event);
            },
            // 2. Sabit Adım Güncelleme Callback
            [&](float dt) {
                input.update();
                scene.update(dt, input);
            },
            // 3. Değişken Oran Render Callback
            [&](float alpha) {
                scene.render(window, alpha);
            }
        );

        scene.onExit();
    } catch (const std::exception &e) {
        std::cerr << "[HATA] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
