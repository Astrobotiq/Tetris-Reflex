// ─────────────────────────────────────────────────────────────────────────────
// main.cpp  —  Program entry point
//
// main() is intentionally tiny. Its only job is to:
//   1. Construct the Application (owns window, loop, input, scenes).
//   2. Push the starting scene.
//   3. Call run() and let the engine take over.
//
// All engine logic lives in the subsystems. main() is just the ignition key.
// ─────────────────────────────────────────────────────────────────────────────

#include "Application.hpp"
#include "GameScene.hpp"
#include <iostream>

int main() {
    try {
        // Construct the engine. The RenderWindow is created inside Application.
        // Width=800, Height=600, locked to 60 logic-updates/sec.
        engine::Application app("SimpleEngine2D  |  WASD to move, Q/E to rotate", 800, 600, 60);

        // Push the first scene. make_unique allocates on the heap and returns
        // a unique_ptr — no raw new/delete anywhere in user code.
        app.pushScene(std::make_unique<GameScene>());

        // Hand control to the game loop. Returns when the window is closed.
        app.run();

    } catch (const std::exception& e) {
        // Catch any std::exception (SFML may throw on graphics init failure).
        std::cerr << "[FATAL] Unhandled exception: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
