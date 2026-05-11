# 🎮 Tetris — SimpleEngine2D

A C++17 / SFML 3 Tetris game built on top of a custom 2D engine, featuring a slot machine power system, drag-and-drop piece placement, and particle effects.

---

## Features

- **Classic Tetris mechanics** — 7 tetrominoes, ghost piece, hard drop, line clearing
- **Slot Machine system** — Earn tokens, spin the slots, hit combos, unlock powers
- **Power system** — 15 unique powers (line clear, time stop, block explosion, and more)
- **Selection Bar** — Drag & drop pieces onto the board at any time
- **Particle effects** — Animations for line clears and piece placements
- **Audio system** — Sound effects and background music (SFML Audio)
- **Level system** — Speed increases every 10 lines; level transitions include a 15-second grace period

---

## Requirements

| Tool | Version |
|------|---------|
| CMake | ≥ 3.17 |
| C++ Compiler | C++17 support (MSVC 2019+, GCC 10+, Clang 12+) |
| SFML | 3.x |
| vcpkg | Any recent version |

---

## Build — Windows (vcpkg + MSVC)

### 1. Install vcpkg (if you haven't already)

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### 2. Install SFML via vcpkg

```bash
C:\vcpkg\vcpkg.exe install sfml:x64-windows
```

### 3. Clone the repository

```bash
git clone https://github.com/your-username/SimpleEngine2D.git
cd SimpleEngine2D
```

### 4. Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

### 5. Run

```bash
build\Release\TetrisGame.exe
```

> The assets folder is automatically copied to `build/Release/assets/` after every build.

---

## Build — Linux / macOS

### Install SFML

**Ubuntu / Debian:**
```bash
sudo apt install libsfml-dev
```

**macOS (Homebrew):**
```bash
brew install sfml
```

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/TetrisGame
```

---

## Project Structure

```
SimpleEngine2D/
├── assets/
│   ├── font.ttf
│   ├── sounds/
│   │   ├── piece_land.wav
│   │   ├── line_clear.wav
│   │   ├── line_clear_4.wav
│   │   ├── level_up.wav
│   │   ├── token_earn.wav
│   │   ├── slot_spin.wav
│   │   └── game_over.wav
│   └── music/
│       └── bgm.ogg
├── src/
│   ├── tetris/
│   │   ├── TetrisScene.hpp     — Main game scene
│   │   ├── GameState.hpp       — Game logic and state
│   │   ├── Board.hpp           — Grid system
│   │   ├── BoardRenderer.hpp   — Board rendering
│   │   ├── Tetromino.hpp       — Piece definitions
│   │   ├── SelectionBar.hpp    — Drag & drop UI
│   │   ├── SlotMachine.hpp     — Token and power system
│   │   └── ParticleEffect.hpp  — Particle effects
│   ├── AudioManager.hpp        — Audio manager
│   ├── AssetManager.hpp        — Asset manager
│   ├── SceneManager.hpp
│   ├── InputManager.hpp
│   ├── Application.cpp
│   ├── GameLoop.cpp
│   └── main_tetris.cpp
└── CMakeLists.txt
```

---

## Controls

| Key / Action | Function |
|-------------|----------|
| `P` | Pause / Resume |
| `R` | Restart (on game over screen) |
| **Left click** (Selection Bar) | Drag a piece |
| **Right click** (Selection Bar) | Rotate a piece |
| **Left click** (Power button) | Use a power |
| **"SPIN" button** | Spin the slot machine (costs 1 token) |

---

## Gameplay

### Token System
- Some falling pieces are highlighted in **gold** — they carry a token
- Clearing a line that contains a token piece awards **1 token**
- Every **1000 points** automatically grants **1 token**
- Tokens are spent on the **SPIN** button (1 token per spin)

### Slot Machine
- 3 slots spin simultaneously
- **2 matching** → small power
- **3 matching** → big power
- **S + Z** → special combination

### Powers

| Combination | Small Power | Big Power |
|-------------|-------------|-----------|
| I × 2/3 | Clear 1 row | Clear 2 rows |
| O × 2/3 | Freeze time 4s | Freeze time 8s |
| T × 2/3 | Refresh 3 piece | Refresh 3 pieces |
| L × 2/3 | Rotate left | Rotate left ×2 |
| J × 2/3 | Rotate right | Rotate right ×2 |
| S × 2/3 | Highest column −1 | 2 columns −1 |
| Z × 2/3 | Explode 1 block | Explode 3 blocks |
| S + Z | — | Highest column −2 |

---

## License

MIT License — free to use, modify, and distribute.
