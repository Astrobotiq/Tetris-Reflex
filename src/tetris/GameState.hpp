#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameState.hpp  —  Oyun mantığı: düşen parça, sıradaki parçalar, skor
//
// ÖZEL MEKANİK: Ağırlıklı rastgelelik (Director AI) ve 7-Bag sistemi.
// ─────────────────────────────────────────────────────────────────────────────

#include "Board.hpp"
#include "Tetromino.hpp"
#include <random>
#include <array>
#include <vector>
#include <optional>
#include <functional>
#include <algorithm>

namespace tetris {
    inline constexpr int SELECTION_SIZE = 3;
    inline constexpr std::array<int, 5> LINE_SCORES = {0, 100, 300, 500, 800};

    struct GameState {
        Board board;

        Piece fallingPiece;
        bool gameOver{false};
        bool paused{false};

        std::array<Piece, SELECTION_SIZE> selectionPieces;

        int score{0};
        int linesCleared{0};
        int level{1};

        float fallInterval{1.0f};
        float fallAccum{0.f};

        bool debugDisableGravity{false};

        std::array<Piece, 3> nextPieces;

        std::function<void(int linesCleared, std::vector<int> clearedRows)> onLinesCleared;
        std::function<void()> onPieceLocked;
        std::function<void()> onGameOver;

        explicit GameState(std::uint32_t seed = 42)
            : m_rng(seed) {

            // İlk başlangıçta torbayı doldur
            refillBag();

            for (auto &p: nextPieces) p = spawnPiece();
            for (auto &p: selectionPieces) p = spawnPiece();
            spawnFallingPiece();
        }

        void update(float dt) {
            if (gameOver || paused) return;

            if (!debugDisableGravity) {
                fallAccum += dt;
                if (fallAccum >= fallInterval) {
                    fallAccum -= fallInterval;
                    stepFallingPiece();
                }
            }
        }

        bool placeSelectionPiece(int selectionIndex, int targetCol, int targetRow) {
            if (selectionIndex < 0 || selectionIndex >= SELECTION_SIZE) return false;

            Piece piece = selectionPieces[selectionIndex];
            auto [valid, placed] = board.getDirectPlacement(piece, targetCol, targetRow);

            if (!valid) return false;

            board.place(placed);
            processLineClears();

            for (int i = 0; i < selectionPieces.size(); ++i) {
                selectionPieces[i] = spawnPiece();
            }
            return true;
        }

        void rotateSelectionPiece(int index) {
            if (index >= 0 && index < SELECTION_SIZE)
                selectionPieces[index].rotateRight();
        }

        void recalcLevel() {
            level = std::max(1, linesCleared / 10 + 1);
            fallInterval = std::max(0.1f, 1.0f - (level - 1) * 0.08f);
        }

    private:
        std::mt19937 m_rng;
        std::vector<TetrominoType> m_bag; // 7-Bag torbası

        // ── 7-Bag Algoritması ────────────────────────────────────────────────────
        void refillBag() {
            m_bag = {
                TetrominoType::I, TetrominoType::J, TetrominoType::L,
                TetrominoType::O, TetrominoType::S, TetrominoType::T, TetrominoType::Z
            };
            std::shuffle(m_bag.begin(), m_bag.end(), m_rng);
        }

        TetrominoType getNextFromBag() {
            if (m_bag.empty()) refillBag();
            TetrominoType type = m_bag.back();
            m_bag.pop_back();
            return type;
        }

        Piece spawnPiece() {
            Piece p;
            p.type = getNextFromBag();

            std::uniform_int_distribution<int> rotDist(0, 3);
            p.rotation = rotDist(m_rng);

            p.col = BOARD_COLS / 2 - 2;
            p.row = 0;
            return p;
        }

        // ── Director AI (Ağırlıklı X Ekseni Seçimi) ──────────────────────────────
        int calculateSpawnColumn() {
            const auto& heights = board.getColumnHeights();
            std::array<double, BOARD_COLS> weights;

            // Maximum satırın biraz üstünü referans alıyoruz (örnek: 20.0)
            const double baseWeight = static_cast<double>(BOARD_ROWS) + 2.0;

            for (size_t i = 0; i < BOARD_COLS; ++i) {
                // Alçak sütuna yüksek ağırlık
                double w = baseWeight - static_cast<double>(heights[i]);
                weights[i] = std::max(0.1, w);
            }

            std::discrete_distribution<int> dist(weights.begin(), weights.end());
            return dist(m_rng);
        }

        void spawnFallingPiece() {
            fallingPiece = nextPieces[0];

            // AI'a sor: Hangi sütuna inelim?
            int targetCol = calculateSpawnColumn();

            // Parçanın 4x4 matrix sınırlarını bul
            int minC = 4, maxC = -1;
            for (auto [r, c] : fallingPiece.localCells()) {
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }

            // Pivot hesapla ve hedef sütuna oturt
            fallingPiece.col = targetCol - (minC + maxC) / 2;

            // Sınır taşkınlığını engelle
            fallingPiece.col = std::clamp(fallingPiece.col, -minC, BOARD_COLS - 1 - maxC);
            fallingPiece.row = 0;

            nextPieces[0] = nextPieces[1];
            nextPieces[1] = nextPieces[2];
            nextPieces[2] = spawnPiece();

            if (!board.canPlace(fallingPiece)) {
                gameOver = true;
                if (onGameOver) onGameOver();
            }
        }

        void stepFallingPiece() {
            Piece next = fallingPiece;
            next.row++;

            if (board.canPlace(next)) {
                fallingPiece = next;
            } else {
                board.place(fallingPiece);
                if (onPieceLocked) onPieceLocked();
                processLineClears();
                spawnFallingPiece();
            }
        }

        void processLineClears() {
            std::vector<int> clearedRows;
            for (int r = 0; r < BOARD_ROWS; ++r) {
                if (board.isRowFull(r)) clearedRows.push_back(r);
            }

            int n = board.clearFullRows();
            if (n > 0) {
                int pts = LINE_SCORES[std::min(n, 4)] * level;
                score += pts;
                linesCleared += n;
                recalcLevel();
                if (onLinesCleared) onLinesCleared(n, clearedRows);
            }
        }

        void hardDropFallingPiece() {
            Piece next = fallingPiece;
            while (true) {
                next.row++;
                if (!board.canPlace(next)) break;
                fallingPiece = next;
            }

            board.place(fallingPiece);
            if (onPieceLocked) onPieceLocked();

            processLineClears();
            spawnFallingPiece();

            fallAccum = 0.f;
        }
    };
} // namespace tetris