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
    bool canPlace(const Piece& piece) const {
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

    // ── "Ghost" çizgisi ───────────────────────────────────────────────────────
    Piece ghostPiece(Piece piece) const {
        while (true) {
            Piece next = piece;
            next.row++;
            if (!canPlace(next)) break;
            piece = next;
        }
        return piece;
    }

    // ── Sürüklenen parça için "en iyi yerleşim" ───────────────────────────────
    std::pair<bool, Piece> findDropPosition(Piece piece, int targetCol) const {
        piece.col = targetCol;

        int minC = std::numeric_limits<int>::max();
        int maxC = std::numeric_limits<int>::min();

        for (auto [r, c] : piece.localCells()) {
            minC = std::min(minC, c);
            maxC = std::max(maxC, c);
        }
        if (minC == std::numeric_limits<int>::max()) { minC = 0; maxC = 0; }

        piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);
        piece.row = 0;

        if (!canPlace(piece)) return {false, piece};

        piece = ghostPiece(piece);
        return {true, piece};
    }

    // ── Direkt yerleşim: drag & drop için ────────────────────────────────────
    std::pair<bool, Piece> getDirectPlacement(Piece piece, int targetCol, int targetRow) const {
        int minC = std::numeric_limits<int>::max();
        int maxC = std::numeric_limits<int>::min();

        for (auto [r, c] : piece.localCells()) {
            minC = std::min(minC, c);
            maxC = std::max(maxC, c);
        }
        if (minC == std::numeric_limits<int>::max()) { minC = 0; maxC = 0; }

        piece.col = std::clamp(targetCol, -minC, BOARD_COLS - 1 - maxC);
        piece.row = std::clamp(targetRow, 0, BOARD_ROWS - 1);

        // Orası doluysa yukarı çıkararak güvenli konum ara
        for (int r = piece.row; r >= 0; --r) {
            piece.row = r;
            if (canPlace(piece)) {
                piece = ghostPiece(piece); // Aşağıya yapıştır
                return {true, piece};
            }
        }
        return {false, piece};
    }

private:
    void removeRow(int row) {
        for (int r = row; r > 0; --r)
            grid[r] = grid[r - 1];
        grid[0].fill(EMPTY_COLOR);
    }

    std::array<std::array<CellColor, BOARD_COLS>, BOARD_ROWS> grid;
};

} // namespace tetris