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
#include <array>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <algorithm>

namespace tetris {
    inline constexpr int BOARD_COLS = 10;
    inline constexpr int BOARD_ROWS = 18;
    inline constexpr int CELL_SIZE = 32; // Piksel cinsinden hücre boyutu

    // Hücrede saklanan rengi temsil eder (0 = boş)
    using CellColor = sf::Color;
    inline const CellColor EMPTY_COLOR = sf::Color::Transparent;

    // ─────────────────────────────────────────────────────────────────────────────
    // Board
    // ─────────────────────────────────────────────────────────────────────────────
    class Board {
    public:
        Board() { clear(); }

        void clear() {
            for (auto &row: m_cells)
                row.fill(EMPTY_COLOR);
        }

        // ── Hücre erişimi ────────────────────────────────────────────────────────
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

        void setCell(int col, int row, CellColor color) {
            if (inBounds(col, row)) m_cells[row][col] = color;
        }

        // ── Çarpışma kontrolü ────────────────────────────────────────────────────
        // Bir parçanın verilen konumda tahtaya sığıp sığmadığını kontrol eder.
        bool canPlace(const Piece &piece) const {
            for (auto [r, c]: piece.cells()) {
                if (c < 0 || c >= BOARD_COLS) return false;
                if (r >= BOARD_ROWS) return false;
                if (r < 0) continue; // Üst sınırın üstü geçerli
                if (!isEmpty(c, r)) return false;
            }
            return true;
        }

        // Parçayı tahtaya yerleştir.
        void place(const Piece &piece) {
            for (auto [r, c]: piece.cells()) {
                if (inBounds(c, r)) {
                    m_cells[r][c] = piece.color();
                }
            }
        }

        // ── Satır temizleme ──────────────────────────────────────────────────────
        // Dolu satırları temizler, üstteki satırları aşağı kaydırır.
        // Temizlenen satır sayısını döndürür.
        int clearFullRows() {
            int cleared = 0;
            for (int row = BOARD_ROWS - 1; row >= 0;) {
                if (isRowFull(row)) {
                    removeRow(row);
                    ++cleared;
                    // row'u azaltma: aynı indeks şimdi kaydırılmış satır
                } else {
                    --row;
                }
            }
            return cleared;
        }

        // Belirli bir satırın dolu olup olmadığı
        bool isRowFull(int row) const {
            for (int c = 0; c < BOARD_COLS; ++c) {
                if (isEmpty(c, row)) return false;
            }
            return true;
        }

        // Tahtanın dolu olup olmadığı (oyun bitti mi?)
        // En üst 2 satırda herhangi bir dolu hücre varsa
        bool isTopOut() const {
            for (int row = 0; row < 2; ++row)
                for (int c = 0; c < BOARD_COLS; ++c)
                    if (!isEmpty(c, row)) return true;
            return false;
        }

        // ── "Ghost" çizgisi ───────────────────────────────────────────────────────
        // Düşen parçanın nereye düşeceğini gösterir (en alttaki geçerli konum).
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
            // 1. Parçanın 4x4 içindeki gerçek görsel sınırlarını bul
            int minC = 4;
            int maxC = -1;
            for (auto [r,c]: piece.localCells()) {
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }

            // 2. İŞTE SİHİR BURADA: Pivot Kaydırması
            // Parçanın görsel merkezini buluyoruz.
            int centerOffset = (minC + maxC) / 2;

            // Farenin bulunduğu kolondan bu ofseti çıkararak,
            // parçanın 4x4'lük matrisinin (pivotunun) nerede başlaması gerektiğini buluyoruz.
            piece.col = mouseCol - centerOffset;

            // 3. Sınırlandırma (Clamp)
            // Oyuncu fareyi çok hızlı dışarı atarsa diye parçayı tahta içinde tutuyoruz.
            piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);

            // 4. Yukarıdan aşağıya doğru düşme testi
            piece.row = 0;
            if (!canPlace(piece)) return {false, piece};

            // Ghost mantığıyla en alta indir
            piece = ghostPiece(piece);
            return {true, piece};
        }

        std::pair<bool, Piece> getDirectPlacement(Piece piece, int mouseCol, int mouseRow) const {
            // 1. Parçanın 4x4 matrisindeki gerçek sınırlarını bul (Hem X hem Y için)
            int minC = 4, maxC = -1;
            int minR = 4, maxR = -1;

            for (auto [r, c] : piece.localCells()) {
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
            }

            // Eğer parça boşsa (hata durumu), false dön
            if (maxR < 0) return {false, piece};

            // 2. Pivot Kaydırması (Center Offset)
            // Farenin ucu tam olarak parçanın ortasına denk gelsin diye merkezi buluyoruz.
            int centerX = (minC + maxC) / 2;
            int centerY = (minR + maxR) / 2;

            piece.col = mouseCol - centerX;
            piece.row = mouseRow - centerY; // Y ekseni için pivot kaydırması!

            // 3. Tahta Sınırlandırması (Clamp)
            // Oyuncu taşı tahtanın dışına sürüklerse, taş tahtanın kenarına yapışsın (çökmesin)
            piece.col = std::clamp(piece.col, -minC, BOARD_COLS - 1 - maxC);
            piece.row = std::clamp(piece.row, -minR, BOARD_ROWS - 1 - maxR); // Y ekseni sınırlandırması!

            // 4. Çarpışma Testi
            // Artık yukarıdan aşağı inmek (ghost) yok. Tam o noktada yer var mı?
            if (canPlace(piece)) {
                return {true, piece}; // Yer var, kopyalanmış ve güncellenmiş piece'i dön
            }

            return {false, piece}; // Yer yok (altında başka blok var vs.)
        }

    private:
        void removeRow(int row) {
            // Satırı sil: üstteki tüm satırları bir aşağı kaydır
            for (int r = row; r > 0; --r)
                m_cells[r] = m_cells[r - 1];
            m_cells[0].fill(EMPTY_COLOR);
        }

        // Row-major: m_cells[row][col]
        std::array<std::array<CellColor, BOARD_COLS>, BOARD_ROWS> m_cells;
    };
} // namespace tetris
