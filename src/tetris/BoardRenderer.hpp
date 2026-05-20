#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// BoardRenderer.hpp  —  Tahta, parçalar, ghost ve grid çizimi
// ─────────────────────────────────────────────────────────────────────────────

#include "Board.hpp"
#include "Tetromino.hpp"
#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>
#include <cmath>

namespace tetris {
    class BoardRenderer {
    public:
        // boardOrigin: tahtanın sol-üst köşesinin ekran koordinatı
        explicit BoardRenderer(sf::Vector2f boardOrigin)
            : m_origin(boardOrigin) {
            // Izgara çizgisi rengi
            m_gridColor = sf::Color(50, 50, 70, 180);

            // Arkaplan
            m_background.setSize({BOARD_COLS * CELL_SIZE, BOARD_ROWS * CELL_SIZE});
            m_background.setPosition(boardOrigin);
            m_background.setFillColor(sf::Color(15, 15, 28));
            m_background.setOutlineColor(sf::Color(80, 80, 120));
            m_background.setOutlineThickness(2.f);
        }

        // Sürüklenen parçanın önizlemesini ayarla (null = gösterme)
        void setDragPreview(std::optional<Piece> piece, bool valid) {
            m_dragPreview = piece;
            m_dragPreviewValid = valid;
        }

        // Parlayan satırları ayarla (satır temizleme animasyonu için)
        void setFlashRows(const std::vector<int> &rows, float alpha) {
            m_flashRows = rows;
            m_flashAlpha = alpha;
        }

        void render(sf::RenderWindow &window, const Board &board,
                    const Piece &falling, bool showGhost = true) {
            window.draw(m_background);
            drawGrid(window);
            drawBoard(window, board);

            // Ghost (düşme önizlemesi)
            if (showGhost) {
                Piece ghost = board.ghostPiece(falling);
                drawPieceCells(window, ghost, sf::Color(255, 255, 255, 40), true);
            }

            // Düşen parça
            drawPieceCells(window, falling, falling.color(), false);

            // Sürükle önizlemesi
            if (m_dragPreview.has_value()) {
                sf::Color previewColor = m_dragPreviewValid
                                             ? sf::Color(255, 255, 255, 130)
                                             : sf::Color(255, 60, 60, 100);
                drawPieceCells(window, *m_dragPreview, previewColor, !m_dragPreviewValid);
            }

            // Flash animasyonu
            if (!m_flashRows.empty() && m_flashAlpha > 0.f) {
                sf::RectangleShape row({float(BOARD_COLS * CELL_SIZE), float(CELL_SIZE)});
                for (int r: m_flashRows) {
                    row.setPosition({m_origin.x, m_origin.y + r * CELL_SIZE});
                    row.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(m_flashAlpha * 255)));
                    window.draw(row);
                }
            }
        }

        int screenXToCol(float screenX) const {
            // std::floor, negatif değerleri doğru bir şekilde aşağıya (-1, -2 vb.) yuvarlar.
            return static_cast<int>(std::floor((screenX - m_origin.x) / CELL_SIZE));
        }

        sf::Vector2f boardToScreen(int col, int row) const {
            return {m_origin.x + col * CELL_SIZE, m_origin.y + row * CELL_SIZE};
        }

        sf::FloatRect boardRect() const {
            return sf::FloatRect(
                {m_origin.x, m_origin.y},
                {float(BOARD_COLS * CELL_SIZE), float(BOARD_ROWS * CELL_SIZE)}
            );
        }

        void setColors(sf::Color bg, sf::Color grid, sf::Color border) {
            m_background.setFillColor(bg);
            m_gridColor = grid;
            m_background.setOutlineColor(border);
        }

    private:
        void drawGrid(sf::RenderWindow &window) {
            sf::RectangleShape line;
            line.setFillColor(m_gridColor);

            // Dikey çizgiler
            line.setSize({1.f, float(BOARD_ROWS * CELL_SIZE)});
            for (int c = 0; c <= BOARD_COLS; ++c) {
                line.setPosition({m_origin.x + c * CELL_SIZE, m_origin.y});
                window.draw(line);
            }
            // Yatay çizgiler
            line.setSize({float(BOARD_COLS * CELL_SIZE), 1.f});
            for (int r = 0; r <= BOARD_ROWS; ++r) {
                line.setPosition({m_origin.x, m_origin.y + r * CELL_SIZE});
                window.draw(line);
            }
        }

        void drawBoard(sf::RenderWindow &window, const Board &board) {
            sf::RectangleShape cell({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
            for (int r = 0; r < BOARD_ROWS; ++r) {
                for (int c = 0; c < BOARD_COLS; ++c) {
                    auto color = board.getCell(c, r);
                    if (color == EMPTY_COLOR) continue;
                    cell.setPosition({
                        m_origin.x + c * CELL_SIZE + 1.f,
                        m_origin.y + r * CELL_SIZE + 1.f
                    });
                    cell.setFillColor(color);
                    // İç parlama efekti
                    cell.setOutlineColor(lighten(color, 60));
                    cell.setOutlineThickness(-1.5f);
                    window.draw(cell);
                }
            }
        }

        void drawPieceCells(sf::RenderWindow &window, const Piece &piece,
                            sf::Color color, bool outline) {
            sf::RectangleShape cell({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
            cell.setFillColor(color);
            if (outline) {
                cell.setOutlineColor(sf::Color(color.r, color.g, color.b, 120));
                cell.setOutlineThickness(1.5f);
                cell.setFillColor(sf::Color(color.r, color.g, color.b,
                                            color.a > 80 ? 40 : color.a));
            } else {
                cell.setOutlineColor(lighten(color, 60));
                cell.setOutlineThickness(-1.5f);
            }

            for (auto [r, c]: piece.cells()) {
                if (r < 0) continue; // Üst sınır dışı
                cell.setPosition({
                    m_origin.x + c * CELL_SIZE + 1.f,
                    m_origin.y + r * CELL_SIZE + 1.f
                });
                window.draw(cell);
            }
        }

        static sf::Color lighten(sf::Color c, int amount) {
            return sf::Color(
                static_cast<std::uint8_t>(std::min(255, c.r + amount)),
                static_cast<std::uint8_t>(std::min(255, c.g + amount)),
                static_cast<std::uint8_t>(std::min(255, c.b + amount)),
                c.a);
        }

        sf::Vector2f m_origin;
        sf::Color m_gridColor;
        sf::RectangleShape m_background;

        std::optional<Piece> m_dragPreview;
        bool m_dragPreviewValid{true};

        std::vector<int> m_flashRows;
        float m_flashAlpha{0.f};
    };
} // namespace tetris
