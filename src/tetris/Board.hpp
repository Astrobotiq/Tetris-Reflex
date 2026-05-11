#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Board.hpp  —  Oyun tahtası: grid durumu, çarpışma, satır temizleme
//
// Board koordinat sistemi:
//   col 0..COLS-1  →  x (sol = 0)
//   row 0..ROWS-1  →  y (üst = 0)
//
// Hücre değerleri:
//   0              = boş
//   1-7            = yerleşmiş blok rengi (TetrominoType+1)
//   FALLING_MARK   = düşen parçanın geçici izi (çizim amaçlı)
// ─────────────────────────────────────────────────────────────────────────────

#include "Tetromino.hpp"
#include <limits>    // std::numeric_limits
#include <array>
#include <vector>
#include <algorithm> // std::min, std::max, std::clamp
#include <SFML/Graphics/Color.hpp>

namespace tetris {

inline constexpr int BOARD_COLS  = 10;
inline constexpr int BOARD_ROWS  = 18;
inline constexpr int CELL_SIZE   = 32;   // Piksel cinsinden hücre boyutu

// Hücrede saklanan rengi temsil eder
using CellColor = sf::Color;
inline const CellColor EMPTY_COLOR = sf::Color::Transparent;

// ─────────────────────────────────────────────────────────────────────────────
// Board
// ─────────────────────────────────────────────────────────────────────────────
class Board {
public:
    Board() { clear(); }

    void clear() {
        for (auto& row : grid)
            row.fill(EMPTY_COLOR);
    }

    // ── Hücre erişimi ────────────────────────────────────────────────────────
    bool isEmpty(int col, int row) const {
        if (!inBounds(col, row)) return false;
        return grid[row][col] == EMPTY_COLOR;
    }

    bool inBounds(int col, int row) const {
        return col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS;
    }

    CellColor getCell(int col, int row) const {
        if (!inBounds(col, row)) return EMPTY_COLOR;
        return grid[row][col];
    }

    void setCell(int col, int row, CellColor color) {
        if (inBounds(col, row)) grid[row][col] = color;
    }

    // ── Çarpışma kontrolü ────────────────────────────────────────────────────
    bool canPlace(const Piece& piece, const Piece* fallingPiece = nullptr) const {

        // 1. Eğer düşen parça gönderildiyse, sürüklenen parça ile çakışıyor mu kontrol et
        if (fallingPiece != nullptr) {
            auto draggedCells = piece.cells();
            auto fallingCells = fallingPiece->cells();

            for (const auto& dCell : draggedCells) {
                for (const auto& fCell : fallingCells) {
                    if (dCell == fCell) {
                        return false; // Düşen parça ile çakışma var!
                    }
                }
            }
        }

        // 2. Tahta sınırları ve yerleşmiş bloklarla çarpışma kontrolü
        for (auto [r, c] : piece.cells()) {
            if (c < 0 || c >= BOARD_COLS) return false;
            if (r >= BOARD_ROWS)          return false;
            if (r < 0)                    continue; // Üst sınırın üstü geçerli
            if (!isEmpty(c, r))           return false;
        }
        return true;
    }

    void place(const Piece& piece) {
        for (auto [r, c] : piece.cells()) {
            if (inBounds(c, r)) {
                grid[r][c] = piece.color();
            }
        }
    }

    // ── Satır temizleme ──────────────────────────────────────────────────────
    int clearFullRows() {
        int cleared = 0;
        for (int row = BOARD_ROWS - 1; row >= 0; ) {
            if (isRowFull(row)) {
                removeRow(row);
                ++cleared;
            } else {
                --row;
            }
        }
        return cleared;
    }

    void removeRow(int row) {
        for (int r = row; r > 0; --r)
            grid[r] = grid[r - 1];
        grid[0].fill(EMPTY_COLOR);
    }

    bool isRowFull(int row) const {
        for (int c = 0; c < BOARD_COLS; ++c) {
            if (isEmpty(c, row)) return false;
        }
        return true;
    }

    bool isTopOut() const {
        for (int row = 0; row < 2; ++row) {
            for (int c = 0; c < BOARD_COLS; ++c) {
                if (!isEmpty(c, row)) return true;
            }
        }
        return false;
    }

    // ── "Ghost" çizgisi (Normal düşen parça için) ─────────────────────────────
    Piece ghostPiece(Piece piece) const {
        while (true) {
            Piece next = piece;
            next.row++;
            if (!canPlace(next)) break;
            piece = next;
        }
        return piece;
    }

    // ── Sürüklenen parça için "en iyi yerleşim" (Puzzle Tarzı Serbest Mod) ────
    std::pair<bool, Piece> findDropPosition(Piece piece, int targetCol, int targetRow, const Piece* fallingPiece = nullptr) const {
        int minC = std::numeric_limits<int>::max();
        int maxC = std::numeric_limits<int>::min();
        int minR = std::numeric_limits<int>::max();
        int maxR = std::numeric_limits<int>::min();

        for (auto [r, c] : piece.localCells()) {
            minC = std::min(minC, c);
            maxC = std::max(maxC, c);
            minR = std::min(minR, r);
            maxR = std::max(maxR, r);
        }
        if (minC == std::numeric_limits<int>::max()) {
            minC = 0; maxC = 0; minR = 0; maxR = 0;
        }

        int centerOffsetX = minC + (maxC - minC + 1) / 2;
        int centerOffsetY = minR + (maxR - minR + 1) / 2;

        piece.col = targetCol - centerOffsetX;
        piece.row = targetRow - centerOffsetY;

        piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);
        piece.row = std::clamp(piece.row, -maxR, BOARD_ROWS - 1 - maxR);

        if (!canPlace(piece, fallingPiece)) return {false, piece};

        return {true, piece};
    }

    // ── Direkt yerleşim: drag & drop bırakma anı (Puzzle Tarzı) ───────────────
    std::pair<bool, Piece> getDirectPlacement(Piece piece, int targetCol, int targetRow, const Piece* fallingPiece = nullptr) const {
        return findDropPosition(piece, targetCol, targetRow, fallingPiece);
    }

private:


    std::array<std::array<CellColor, BOARD_COLS>, BOARD_ROWS> grid;
};

} // namespace tetris