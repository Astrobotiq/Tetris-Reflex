# 🎮 Tetris — SimpleEngine2D

Klasik Tetris'in üzerine inşa edilmiş, slot machine ve güç sistemi içeren C++17 / SFML 3 oyunu.

---

## Özellikler

- **Klasik Tetris mekaniği** — 7 tetromino, ghost piece, hard drop, satır temizleme
- **Slot Machine sistemi** — Jeton biriktir, slot çevir, kombo yap, güç kazan
- **Güç sistemi** — 15 farklı güç (satır temizleme, zaman durdurma, blok patlatma vb.)
- **Selection Bar** — Sürükle & bırak ile tahtaya parça yerleştir
- **Parçacık efektleri** — Satır temizleme ve yerleştirme animasyonları
- **Ses sistemi** — Efektler ve arka plan müziği (SFML Audio)
- **Level sistemi** — Her 10 satırda level atla, hız artar; level geçişinde 15 saniyelik bekleme

---

## Gereksinimler

| Araç | Sürüm |
|------|-------|
| CMake | ≥ 3.17 |
| C++ Derleyici | C++17 destekli (MSVC 2019+, GCC 10+, Clang 12+) |
| SFML | 3.x |
| vcpkg | Herhangi bir güncel sürüm |

---

## Build — Windows (vcpkg + MSVC)

### 1. vcpkg kurulumu (daha önce yapmadıysan)

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### 2. SFML'i vcpkg ile yükle

```bash
C:\vcpkg\vcpkg.exe install sfml:x64-windows
```

### 3. Projeyi klonla

```bash
git clone https://github.com/kullanici-adin/SimpleEngine2D.git
cd SimpleEngine2D
```

### 4. Build et

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

### 5. Çalıştır

```bash
build\Release\TetrisGame.exe
```

> Assets klasörü build sonrası otomatik olarak `build/Release/assets/` altına kopyalanır.

---

## Build — Linux / macOS

### SFML kurulumu

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

## Proje Yapısı

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
│   │   ├── TetrisScene.hpp     — Ana oyun sahnesi
│   │   ├── GameState.hpp       — Oyun durumu ve mantığı
│   │   ├── Board.hpp           — Tahta grid sistemi
│   │   ├── BoardRenderer.hpp   — Tahta çizimi
│   │   ├── Tetromino.hpp       — Parça tanımları
│   │   ├── SelectionBar.hpp    — Sürükle & bırak arayüzü
│   │   ├── SlotMachine.hpp     — Jeton ve güç sistemi
│   │   └── ParticleEffect.hpp  — Parçacık efektleri
│   ├── AudioManager.hpp        — Ses yöneticisi
│   ├── AssetManager.hpp        — Varlık yöneticisi
│   ├── SceneManager.hpp
│   ├── InputManager.hpp
│   ├── Application.cpp
│   ├── GameLoop.cpp
│   └── main_tetris.cpp
└── CMakeLists.txt
```

---

## Kontroller

| Tuş / Eylem | İşlev |
|-------------|-------|
| **Sol tık** (Selection Bar) | Parçayı sürükle |
| **Sağ tık** (Selection Bar) | Parçayı döndür |
| **Sol tık** (Güç butonu) | Güç kullan |
| **"CEVİR" butonu** | Slot machine çevir (1 jeton) |

---

## Oyun Mekaniği

### Jeton Sistemi
- Bazı düşen parçalar **altın rengi** ile işaretlenir — jeton taşır
- O parça dahil bir satır temizlenirse **1 jeton** kazanılır
- Her **1000 puanda** otomatik olarak **1 jeton** verilir
- Jetonla **"CEVİR"** butonuna basılır (1 jeton / çevirme)

### Slot Machine
- 3 slot aynı anda döner
- **2 aynı** → küçük güç
- **3 aynı** → büyük güç
- **S + Z** → özel kombinasyon

### Güçler

| Kombinasyon | Küçük Güç | Büyük Güç |
|-------------|-----------|-----------|
| I × 2/3 | 1 satır temizle | 2 satır temizle |
| O × 2/3 | 4sn zaman durdur | 8sn zaman durdur |
| T × 2/3 | 3 parçayı yenile | 3 parçayı yenile |
| L × 2/3 | Sola döndür | 2× sola döndür |
| J × 2/3 | Sağa döndür | 2× sağa döndür |
| S × 2/3 | En yüksek sütun −1 | 2 sütun −1 |
| Z × 2/3 | 1 blok patlat | 3 blok patlat |
| S + Z | — | En yüksek sütun −2 |

---

## Lisans

MIT License — dilediğin gibi kullanabilirsin.
