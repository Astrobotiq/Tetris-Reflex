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

            for (int r = 0; r < BOARD_ROWS; ++r) {
                for (int c = 0; c < BOARD_COLS; ++c) {
                    m_cellScales[r][c] = 1.f;
                }
            }
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

            // Render active board effects (glowing light spreads)
            for (const auto& fx : m_effects) {
                float progress = fx.timer / fx.duration;
                float alpha = (1.f - progress) * 200.f;
                sf::Color c = fx.color;
                c.a = static_cast<std::uint8_t>(alpha);

                if (fx.type == BoardEffect::Type::LineClear) {
                    float maxDist = BOARD_ROWS * CELL_SIZE * 0.5f;
                    float dist = progress * maxDist;

                    sf::RectangleShape beam({float(BOARD_COLS * CELL_SIZE), 8.f});
                    beam.setOrigin({0.f, 4.f});
                    beam.setFillColor(c);

                    float yUp = fx.centerY - dist;
                    if (yUp >= m_origin.y) {
                        beam.setPosition({m_origin.x, yUp});
                        window.draw(beam);
                    }
                    float yDown = fx.centerY + dist;
                    if (yDown <= m_origin.y + BOARD_ROWS * CELL_SIZE) {
                        beam.setPosition({m_origin.x, yDown});
                        window.draw(beam);
                    }
                } else if (fx.type == BoardEffect::Type::Placement) {
                    float maxRadius = CELL_SIZE * 4.f;
                    float radius = progress * maxRadius;

                    sf::CircleShape ring(radius);
                    ring.setOrigin({radius, radius});
                    ring.setPosition(fx.center);
                    ring.setFillColor(sf::Color::Transparent);
                    ring.setOutlineColor(c);
                    ring.setOutlineThickness(4.f);
                    window.draw(ring);
                }
            }

            // Draw flashing cells about to be destroyed
            for (const auto& fc : m_flashingCells) {
                float progress = fc.timer / fc.duration;
                // High contrast flash: alternate white and original color
                float flashCycle = std::sin(progress * 40.f) * 0.5f + 0.5f;
                std::uint8_t r = static_cast<std::uint8_t>(fc.color.r + (255 - fc.color.r) * flashCycle);
                std::uint8_t g = static_cast<std::uint8_t>(fc.color.g + (255 - fc.color.g) * flashCycle);
                std::uint8_t b = static_cast<std::uint8_t>(fc.color.b + (255 - fc.color.b) * flashCycle);
                sf::Color drawColor(r, g, b, static_cast<std::uint8_t>((1.f - progress) * 255));
                
                sf::RectangleShape cell({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
                cell.setOrigin({(CELL_SIZE - 2.f) * 0.5f, (CELL_SIZE - 2.f) * 0.5f});
                cell.setPosition({
                    m_origin.x + fc.col * CELL_SIZE + CELL_SIZE * 0.5f,
                    m_origin.y + fc.row * CELL_SIZE + CELL_SIZE * 0.5f
                });
                
                float scale = 1.2f * (1.f - progress);
                cell.setScale({scale, scale});
                cell.setFillColor(drawColor);
                cell.setOutlineColor(sf::Color::White);
                cell.setOutlineThickness(-1.f);
                window.draw(cell);
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

        void update(float dt) {
            for (int r = 0; r < BOARD_ROWS; ++r) {
                for (int c = 0; c < BOARD_COLS; ++c) {
                    m_cellScales[r][c] += (1.f - m_cellScales[r][c]) * 15.f * dt;
                }
            }
            for (auto it = m_effects.begin(); it != m_effects.end(); ) {
                it->timer += dt;
                if (it->timer >= it->duration) {
                    it = m_effects.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = m_flashingCells.begin(); it != m_flashingCells.end(); ) {
                it->timer += dt;
                if (it->timer >= it->duration) {
                    it = m_flashingCells.erase(it);
                } else {
                    ++it;
                }
            }
        }

        void triggerLineClearScalePop(const Board& board) {
            for (int r = 0; r < BOARD_ROWS; ++r) {
                for (int c = 0; c < BOARD_COLS; ++c) {
                    if (!board.isEmpty(c, r)) {
                        m_cellScales[r][c] = 1.35f;
                    }
                }
            }
        }

        void triggerPlacementScalePop(const std::vector<std::pair<int, int>>& cells, const Board& board) {
            for (auto pos : cells) {
                for (int dr = -2; dr <= 2; ++dr) {
                    for (int dc = -2; dc <= 2; ++dc) {
                        int r = pos.first + dr;
                        int c = pos.second + dc;
                        if (r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS) {
                            if (!board.isEmpty(c, r)) {
                                m_cellScales[r][c] = 1.25f;
                            }
                        }
                    }
                }
            }
        }

        void triggerLineClearEffect(float centerY, sf::Color color) {
            m_effects.push_back({BoardEffect::Type::LineClear, 0.f, 0.45f, centerY, {0.f, 0.f}, color});
        }

        void triggerPlacementEffect(sf::Vector2f center, sf::Color color) {
            m_effects.push_back({BoardEffect::Type::Placement, 0.f, 0.35f, 0.f, center, color});
        }

        void triggerCellFlash(int col, int row, sf::Color color) {
            m_flashingCells.push_back({col, row, color, 0.f, 0.35f});
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
            cell.setOrigin({(CELL_SIZE - 2.f) * 0.5f, (CELL_SIZE - 2.f) * 0.5f});
            for (int r = 0; r < BOARD_ROWS; ++r) {
                for (int c = 0; c < BOARD_COLS; ++c) {
                    auto color = board.getCell(c, r);
                    if (color == EMPTY_COLOR) continue;

                    float scale = m_cellScales[r][c];
                    cell.setPosition({
                        m_origin.x + c * CELL_SIZE + CELL_SIZE * 0.5f,
                        m_origin.y + r * CELL_SIZE + CELL_SIZE * 0.5f
                    });
                    cell.setScale({scale, scale});
                    cell.setFillColor(color);
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

        float m_cellScales[BOARD_ROWS][BOARD_COLS];

        struct BoardEffect {
            enum class Type { LineClear, Placement };
            Type type;
            float timer{0.f};
            float duration{0.4f};
            float centerY{0.f};
            sf::Vector2f center;
            sf::Color color;
        };
        std::vector<BoardEffect> m_effects;

        struct FlashingCell {
            int col;
            int row;
            sf::Color color;
            float timer{0.f};
            float duration{0.35f};
        };
        std::vector<FlashingCell> m_flashingCells;
    };
} // namespace tetris
