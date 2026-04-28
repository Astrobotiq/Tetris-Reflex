#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameState.hpp  —  Oyun mantığı: düşen parça, sıradaki parçalar, skor
//
// Bu sınıf SFML veya engine'den bağımsızdır — sadece oyun kuralları.
// Render ve input, TetrisScene tarafından yönetilir.
//
// ÖZEL MEKANİK:
//   • Falling piece: otomatik iner, oyuncu hareket ettiremez
//   • Selection bar: 3 parça gösterilir
//   • Drag & drop: oyuncu parçaları sürükleyip tahtaya yerleştirir
//   • Yerleşim sonrası satır kontrolü yapılır
// ─────────────────────────────────────────────────────────────────────────────

#include "Board.hpp"
#include "Tetromino.hpp"
#include <random>
#include <array>
#include <optional>
#include <functional>

namespace tetris {
    inline constexpr int SELECTION_SIZE = 3; // Alt barda kaç parça gösterilir

    // Skor tablosu (Tetris klasiği, level çarpanı eklendi)
    inline constexpr std::array<int, 5> LINE_SCORES = {0, 100, 300, 500, 800};

    // ─────────────────────────────────────────────────────────────────────────────
    struct GameState {
        Board board;

        // Düşen parça
        Piece fallingPiece;
        bool gameOver{false};
        bool paused{false};

        // Alt çubuk: seçim parçaları
        std::array<Piece, SELECTION_SIZE> selectionPieces;

        // Skor & level
        int score{0};
        int linesCleared{0};
        int level{1};

        // Düşme hızı (saniye/hücre)
        float fallInterval{1.0f};
        float fallAccum{0.f};

        bool debugDisableGravity{false};

        // Sonraki 3 parça kuyruğu (preview için)
        std::array<Piece, 3> nextPieces;

        // Callback: satır temizlendiğinde dış sistemlere bildir (efekt, ses vb.)
        std::function<void(int linesCleared, std::vector<int> clearedRows)> onLinesCleared;
        std::function<void()> onPieceLocked;
        std::function<void()> onGameOver;

        explicit GameState(std::uint32_t seed = 42)
            : m_rng(seed) {
            // Başlangıçta tüm kuyruğu doldur
            for (auto &p: nextPieces) p = spawnPiece();
            for (auto &p: selectionPieces) p = spawnPiece();
            spawnFallingPiece();
        }

        // ── Sabit adım güncelleme ────────────────────────────────────────────────
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

        // ── Seçim parçasını tahtaya sürükle-bırak ───────────────────────────────
        // selectionIndex: 0-2
        // targetCol: bırakılan board sütunu
        // Döndürür: true = başarıyla yerleştirildi
        // ── Seçim parçasını tahtaya sürükle-bırak ───────────────────────────────
        // YENİ: Parametrelere 'int targetRow' eklendi
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

            //hardDropFallingPiece();
            return true;
        }

        // Seçim parçasını döndür
        void rotateSelectionPiece(int index) {
            if (index >= 0 && index < SELECTION_SIZE)
                selectionPieces[index].rotateRight();
        }

        // Level hesapla (her 10 satırda bir seviye artar)
        void recalcLevel() {
            level = std::max(1, linesCleared / 10 + 1);
            // Hız formülü: 1.0 saniyeden başlayıp level arttıkça azalır
            fallInterval = std::max(0.1f, 1.0f - (level - 1) * 0.08f);
        }

    private:
        std::mt19937 m_rng;

        Piece spawnPiece() {
            Piece p;
            p.type = randomType(m_rng);

            std::uniform_int_distribution<int> rotDist(0, 3);
            p.rotation = rotDist(m_rng);

            p.col = BOARD_COLS / 2 - 2;
            p.row = 0;
            return p;
        }

        void spawnFallingPiece() {
            fallingPiece = nextPieces[0];

            std::uniform_int_distribution<int> colDist(0, BOARD_COLS - 4);
            fallingPiece.col = colDist(m_rng);

            fallingPiece.row = 0;

            // Kuyruğu kaydır
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
                // Parça kilitlendi
                board.place(fallingPiece);
                if (onPieceLocked) onPieceLocked();
                processLineClears();
                spawnFallingPiece();
            }
        }

        void processLineClears() {
            // Temizlenecek satırları bul
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
