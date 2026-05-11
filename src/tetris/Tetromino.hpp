#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Tetromino.hpp  —  Tetromino şekil tanımları ve döndürme mantığı
//
// Her tetromino:
//   • 4×4 bitmask array ile tanımlı (4 döndürme × 16 bit)
//   • Renk bilgisi taşır
//   • Hücre koordinatları olarak sorgulanabilir
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <string>
#include <random>

namespace tetris {
    // 7 klasik tetromino tipi
    enum class TetrominoType : int {
        I = 0, O, T, S, Z, J, L,
        COUNT
    };

    // Her tip için 4 döndürme × 4×4 grid (16 bit bitmask, soldan-sağa yukarıdan-aşağı)
    // '1' = dolu hücre, '0' = boş
    struct TetrominoData {
        // rotations[r][row] = 4-bitlik satır (bit3=sol, bit0=sağ)
        std::array<std::array<std::uint8_t, 4>, 4> rotations;
        sf::Color color;
        std::string name;
    };

    // ─── Tüm tetromino verisi ────────────────────────────────────────────────────
    inline const std::array<TetrominoData, 7> TETROMINO_DEFS = {
        {
            // I  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b1111, 0b0000, 0b0000}, // rot 0: ████
                        {0b0010, 0b0010, 0b0010, 0b0010}, // rot 1: dikey
                        {0b0000, 0b1111, 0b0000, 0b0000}, // rot 2: aynı
                        {0b0010, 0b0010, 0b0010, 0b0010}, // rot 3: aynı
                    }
                },
                sf::Color(0, 240, 240), "I"
            },

            // O  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0110, 0b0110, 0b0000, 0b0000},
                        {0b0110, 0b0110, 0b0000, 0b0000},
                        {0b0110, 0b0110, 0b0000, 0b0000},
                        {0b0110, 0b0110, 0b0000, 0b0000},
                    }
                },
                sf::Color(240, 240, 0), "O"
            },

            // T  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b0100, 0b1110, 0b0000},
                        {0b0000, 0b0100, 0b0110, 0b0100},
                        {0b0000, 0b0000, 0b1110, 0b0100},
                        {0b0000, 0b0100, 0b1100, 0b0100},
                    }
                },
                sf::Color(160, 0, 240), "T"
            },

            // S  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b0110, 0b1100, 0b0000},
                        {0b0000, 0b0100, 0b0110, 0b0010},
                        {0b0000, 0b0110, 0b1100, 0b0000},
                        {0b0000, 0b0100, 0b0110, 0b0010},
                    }
                },
                sf::Color(0, 240, 0), "S"
            },

            // Z  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b1100, 0b0110, 0b0000},
                        {0b0000, 0b0010, 0b0110, 0b0100},
                        {0b0000, 0b1100, 0b0110, 0b0000},
                        {0b0000, 0b0010, 0b0110, 0b0100},
                    }
                },
                sf::Color(240, 0, 0), "Z"
            },

            // J  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b1000, 0b1110, 0b0000},
                        {0b0000, 0b0110, 0b0100, 0b0100},
                        {0b0000, 0b0000, 0b1110, 0b0010},
                        {0b0000, 0b0100, 0b0100, 0b1100},
                    }
                },
                sf::Color(0, 0, 240), "J"
            },

            // L  ────────────────────────────────────────────────────────────────────
            {
                std::array<std::array<std::uint8_t, 4>, 4>{
                    {
                        {0b0000, 0b0010, 0b1110, 0b0000},
                        {0b0000, 0b0100, 0b0100, 0b0110},
                        {0b0000, 0b0000, 0b1110, 0b1000},
                        {0b0000, 0b1100, 0b0100, 0b0100},
                    }
                },
                sf::Color(240, 160, 0), "L"
            },
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Piece  —  Çalışma zamanı tetromino örneği (tip + döndürme + konum)
    // ─────────────────────────────────────────────────────────────────────────────
    struct Piece {
        TetrominoType type{TetrominoType::I};
        int rotation{0}; // 0-3
        int col{0}; // Board sütun ofseti
        int row{0}; // Board satır ofseti
        bool hasToken{false}; // Bu parça jeton taşıyor mu?

        const TetrominoData &data() const {
            return TETROMINO_DEFS[static_cast<int>(type)];
        }

        sf::Color color() const {
            // Eğer jetonluysa belirgin bir altın sarısı dön, değilse normal rengini dön
            if (hasToken) return sf::Color(255, 220, 50);
            return data().color;
        }

        // Bu dönüşümdeki dolu hücreleri (satır, sütun) çiftleri olarak döndür.
        // Board koordinatlarında: (row + dr, col + dc)
        std::vector<std::pair<int, int> > cells() const {
            std::vector<std::pair<int, int> > result;
            const auto &rot = data().rotations[rotation];
            for (int dr = 0; dr < 4; ++dr) {
                std::uint8_t rowMask = rot[dr];
                for (int dc = 0; dc < 4; ++dc) {
                    // Bit3 = sol sütun (dc=0), Bit0 = sağ sütun (dc=3)
                    if (rowMask & (0b1000 >> dc)) {
                        result.push_back({row + dr, col + dc});
                    }
                }
            }
            return result;
        }

        // Parçanın kapladığı minimum bounding box (local, 0-tabanlı)
        std::vector<std::pair<int, int> > localCells() const {
            Piece tmp = *this;
            tmp.row = 0;
            tmp.col = 0;
            return tmp.cells();
        }

        void rotateRight() { rotation = (rotation + 1) % 4; }
        void rotateLeft() { rotation = (rotation + 3) % 4; }
    };

    // Rastgele tip üret
    inline TetrominoType randomType(std::mt19937 &rng) {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(TetrominoType::COUNT) - 1);
        return static_cast<TetrominoType>(dist(rng));
    }

    // 0 ile 3 arasında rastgele bir rotasyon (döndürme) değeri üretir
    inline int randomRotation(std::mt19937 &rng) {
        std::uniform_int_distribution<int> dist(0, 3);
        return dist(rng);
    }

    // Hem tipi hem de rotasyonu rastgele olan yepyeni bir parça (Piece) üretir
    inline Piece generateRandomPiece(std::mt19937 &rng) {
        Piece p;
        p.type = randomType(rng);
        p.rotation = randomRotation(rng);

        // p.col ve p.row değerlerini sıfırda bırakıyoruz.
        // Parçanın tahtadaki başlangıç konumunu GameState veya Spawner sistemi belirlemeli.
        return p;
    }
} // namespace tetris