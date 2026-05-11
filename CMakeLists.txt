cmake_minimum_required(VERSION 3.17)
project(SimpleEngine2D VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SFML 3 COMPONENTS Graphics Audio REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# EngineLib — paylaşılan engine kütüphanesi
# ─────────────────────────────────────────────────────────────────────────────
add_library(EngineLib STATIC
    src/Application.cpp
    src/GameLoop.cpp
    src/InputManager.cpp
    src/SceneManager.cpp
    src/RenderSystem.cpp
)
target_include_directories(EngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(EngineLib PUBLIC SFML::Graphics SFML::Audio)
if(MSVC)
    target_compile_options(EngineLib PRIVATE /W4 /EHsc)
else()
    target_compile_options(EngineLib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# SimpleEngine2D — orijinal platformer demo
# GEÇİCİ OLARAK KAPATILDI (Tetris'e odaklanmak için)
# ─────────────────────────────────────────────────────────────────────────────
# add_executable(SimpleEngine2D src/main.cpp)
# target_link_libraries(SimpleEngine2D PRIVATE EngineLib)

# ─────────────────────────────────────────────────────────────────────────────
# TetrisGame — Tetris oyunu
# Tüm Tetris kodu header-only (src/tetris/); yalnızca main_tetris.cpp derlenir.
# ─────────────────────────────────────────────────────────────────────────────
add_executable(TetrisGame src/main_tetris.cpp
        src/tetris/Board.hpp
        src/tetris/SlotMachine.hpp
        src/AudioManager.hpp
        src/AssetManager.hpp
)
target_link_libraries(TetrisGame PRIVATE EngineLib)

if(MSVC)
    target_compile_options(TetrisGame PRIVATE /W4 /EHsc)
else()
    target_compile_options(TetrisGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Assets kopyala (Sadece TetrisGame hedefi bırakıldı)
foreach(TARGET_NAME TetrisGame)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
        COMMENT "Copying assets to ${TARGET_NAME}"
    )
endforeach()