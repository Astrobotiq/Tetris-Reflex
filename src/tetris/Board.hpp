#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Board.hpp  —  Oyun tahtası: grid durumu, çarpışma, satır temizleme, ısı haritası
//
// Board koordinat sistemi:
//   col 0..COLS-1  →  x (sol = 0)
//   row 0..ROWS-1  →  y (üst = 0)
// ─────────────────────────────────────────────────────────────────────────────

#include "Tetromino.hpp"
#include <array>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <algorithm>
#include <cstdint>

namespace tetris {
    inline constexpr int BOARD_COLS = 10;
    inline constexpr int BOARD_ROWS = 18;
    inline constexpr int CELL_SIZE = 32;

    using CellColor = sf::Color;
    inline const CellColor EMPTY_COLOR = sf::Color::Transparent;

    class Board {
    public:
        Board() { clear(); }

        void clear() {
            for (auto &row: m_cells) {
                row.fill(EMPTY_COLOR);
            }
            m_columnHeights.fill(0);
        }

        // ── Veri Okuma ───────────────────────────────────────────────────────────
        bool isEmpty(int col, int row) const {
            if (!inBounds(col, row)) return false;
            return m_cells[row][col] == EMPTY_COLOR;
        }

        bool inBounds(int col, int row) const {
            return col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS;
        }

        CellColor getCell(int col, int row) const {
            if (!inBounds(col, row)) return EMPTY_COLOR;
            return m_cells[row][col];
        }

        // Ağırlıklı rastgelelik (Director) için dışarıdan okunacak
        const std::array<uint8_t, BOARD_COLS>& getColumnHeights() const {
            return m_columnHeights;
        }

        // ── Çarpışma ve Yerleştirme ──────────────────────────────────────────────
        bool canPlace(const Piece &piece) const {
            for (auto [r, c]: piece.cells()) {
                if (c < 0 || c >= BOARD_COLS) return false;
                if (r >= BOARD_ROWS) return false;
                if (r < 0) continue;
                if (!isEmpty(c, r)) return false;
            }
            return true;
        }

        void place(const Piece &piece) {
            for (auto [r, c]: piece.cells()) {
                if (inBounds(c, r)) {
                    m_cells[r][c] = piece.color();

                    // Isı Haritası (Heatmap) Güncellemesi
                    // Y (row) ekseni aşağı doğru büyüdüğü için, yükseklik hesabı tersinedir.
                    // En alt satır(17) = yükseklik 1. En üst satır(0) = yükseklik 18.
                    uint8_t blockHeight = static_cast<uint8_t>(BOARD_ROWS - r);
                    if (blockHeight > m_columnHeights[c]) {
                        m_columnHeights[c] = blockHeight;
                    }
                }
            }
        }

        // ── Satır temizleme ve Isı Haritası Yenileme ─────────────────────────────
        void recalculateHeatmap() {
            m_columnHeights.fill(0);
            for (int c = 0; c < BOARD_COLS; ++c) {
                for (int r = 0; r < BOARD_ROWS; ++r) {
                    if (!isEmpty(c, r)) {
                        m_columnHeights[c] = static_cast<uint8_t>(BOARD_ROWS - r);
                        break; // Sütundaki en yüksek (ilk rastlanan) dolu bloğu bulduk
                    }
                }
            }
        }

        int clearFullRows() {
            int cleared = 0;
            for (int row = BOARD_ROWS - 1; row >= 0;) {
                if (isRowFull(row)) {
                    removeRow(row);
                    ++cleared;
                } else {
                    --row;
                }
            }

            // Satır temizlendiyse tüm ısı haritasını baştan hesapla
            if (cleared > 0) {
                recalculateHeatmap();
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
            for (int row = 0; row < 2; ++row)
                for (int c = 0; c < BOARD_COLS; ++c)
                    if (!isEmpty(c, row)) return true;
            return false;
        }

        // ── Ghost ve Sürükle-Bırak Mantığı ───────────────────────────────────────
        Piece ghostPiece(Piece piece) const {
            while (true) {
                Piece next = piece;
                next.row++;
                if (!canPlace(next)) break;
                piece = next;
            }
            return piece;
        }

        std::pair<bool, Piece> findDropPosition(Piece piece, int mouseCol) const {
            int minC = 4, maxC = -1;
            for (auto [r,c]: piece.localCells()) {
                minC = std::min(minC, c); maxC = std::max(maxC, c);
            }
            int centerOffset = (minC + maxC) / 2;
            piece.col = mouseCol - centerOffset;
            piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);
            piece.row = 0;
            if (!canPlace(piece)) return {false, piece};
            piece = ghostPiece(piece);
            return {true, piece};
        }

        std::pair<bool, Piece> getDirectPlacement(Piece piece, int mouseCol, int mouseRow) const {
            int minC = 4, maxC = -1, minR = 4, maxR = -1;
            for (auto [r, c] : piece.localCells()) {
                minC = std::min(minC, c); maxC = std::max(maxC, c);
                minR = std::min(minR, r); maxR = std::max(maxR, r);
            }
            if (maxR < 0) return {false, piece};

            int centerX = (minC + maxC) / 2;
            int centerY = (minR + maxR) / 2;
            piece.col = mouseCol - centerX;
            piece.row = mouseRow - centerY;

            piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);
            piece.row = std::clamp(piece.row, -minR, BOARD_ROWS - 1 - maxR);

            if (canPlace(piece)) {
                return {true, piece};
            }
            return {false, piece};
        }

    private:
        void removeRow(int row) {
            for (int r = row; r > 0; --r)
                m_cells[r] = m_cells[r - 1];
            m_cells[0].fill(EMPTY_COLOR);
        }

        std::array<std::array<CellColor, BOARD_COLS>, BOARD_ROWS> m_cells;
        std::array<uint8_t, BOARD_COLS> m_columnHeights{}; // SIFIR başlatıldı (C++11/14+)
    };
} // namespace tetris