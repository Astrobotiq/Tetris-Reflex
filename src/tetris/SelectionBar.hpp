#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SelectionBar.hpp  —  Alt çubuk: 3 sürüklenebilir tetromino parçası
//
// Mekanik:
//   • 3 parça yan yana gösterilir
//   • Sol tık basılı tutularak sürükleme başlar
//   • Board üzerinde bırakılırsa yerleştirilir
//   • Sağ tık: parçayı döndür
// ─────────────────────────────────────────────────────────────────────────────

#include "Tetromino.hpp"
#include "GameState.hpp"
#include <SFML/Graphics.hpp>
#include <optional>

namespace tetris {
    inline constexpr float BAR_HEIGHT = 120.f;
    inline constexpr float PIECE_PREVIEW_CELL = 24.f; // Preview hücresi boyutu

    // ─── Sürükleme durumu ────────────────────────────────────────────────────────
    struct DragState {
        int selectionIndex{-1}; // Hangi slot sürükleniyor
        Piece piece; // Sürüklenen parça (döndürme dahil)
        sf::Vector2f mousePos; // Mevcut fare konumu
        bool active{false};
    };

    class SelectionBar {
    public:
        SelectionBar(sf::Vector2f origin, float width)
            : m_origin(origin), m_width(width) {
            m_background.setSize({width, BAR_HEIGHT});
            m_background.setPosition(origin);
            m_background.setFillColor(sf::Color(20, 20, 35));
            m_background.setOutlineColor(sf::Color(80, 80, 130));
            m_background.setOutlineThickness(2.f);
        }

        // Font yükle (yoksa text çizilmez)
        void setFont(const sf::Font &font) { m_font = &font; }

        // ── Input ────────────────────────────────────────────────────────────────

        // Mouse button press: sürükleme başlat
        bool onMousePress(sf::Vector2f mousePos, GameState &gs) {
            int idx = hitTest(mousePos);
            if (idx < 0) return false;

            m_drag.active = true;
            m_drag.selectionIndex = idx;
            m_drag.piece = gs.selectionPieces[idx];
            m_drag.mousePos = mousePos;
            return true;
        }

        // Sağ tık: döndür
        bool onRightClick(sf::Vector2f mousePos, GameState &gs) {
            int idx = hitTest(mousePos);
            if (idx < 0) return false;
            gs.rotateSelectionPiece(idx);
            return true;
        }

        // Mouse hareket: sürükleme pozisyonunu güncelle
        void onMouseMove(sf::Vector2f mousePos) {
            if (m_drag.active) m_drag.mousePos = mousePos;
        }

        // Mouse bırakma: board'a bırak
        // boardOriginX: tahtanın sol x koordinatı
        // Döndürür: (başarılı yerleşim, hangi slot, hangi sütun)
        struct DropResult {
            bool success;
            int slotIndex;
            int boardCol;
        };

        DropResult onMouseRelease(sf::Vector2f mousePos, float boardOriginX,
                                  float boardOriginY, float boardBottom,
                                  GameState &gs) {
            if (!m_drag.active) return {false, -1, -1};
            m_drag.active = false;

            // Fare board alanında mı?
            bool onBoard = mousePos.y >= boardOriginY && mousePos.y <= boardBottom;
            if (!onBoard) return {false, m_drag.selectionIndex, -1};

            int col = static_cast<int>((mousePos.x - boardOriginX) / CELL_SIZE);

            // Parça döndürme durumunu sync et
            int idx = m_drag.selectionIndex;
            gs.selectionPieces[idx].rotation = m_drag.piece.rotation;

            bool ok = gs.placeSelectionPiece(idx, col);
            return {ok, idx, col};
        }

        bool isDragging() const { return m_drag.active; }
        const DragState &dragState() const { return m_drag; }

        // ── Render ───────────────────────────────────────────────────────────────
        void render(sf::RenderWindow &window, const GameState &gs) {
            window.draw(m_background);

            // Her slot için parça preview'ı çiz
            for (int i = 0; i < SELECTION_SIZE; ++i) {
                // Sürüklenen slot: orijin yerinde soluk göster
                bool draggingThis = m_drag.active && m_drag.selectionIndex == i;
                float alpha = draggingThis ? 0.3f : 1.0f;
                drawPiecePreview(window, gs.selectionPieces[i], i, alpha);
                drawSlotBorder(window, i);
            }

            // Sürüklenen parçayı fare pozisyonunda çiz
            if (m_drag.active) {
                drawDraggingPiece(window);
            }

            // Skor/level bilgisi (font varsa)
            if (m_font) {
                drawText(window, "SKOR: " + std::to_string(gs.m_score),
                         {m_origin.x + m_width - 160.f, m_origin.y + 10.f}, 16);
                drawText(window, "SEVIYE: " + std::to_string(gs.level),
                         {m_origin.x + m_width - 160.f, m_origin.y + 34.f}, 16);
                drawText(window, "SATIRLAR: " + std::to_string(gs.linesCleared),
                         {m_origin.x + m_width - 160.f, m_origin.y + 58.f}, 14);

                // Kontrol ipuçları
                drawText(window, "Surukle: Birak   Sag tik: Dondur",
                         {m_origin.x + 10.f, m_origin.y + BAR_HEIGHT - 22.f}, 12,
                         sf::Color(140, 140, 180));
            }
        }

    private:
        // Bir slotun ekran merkezi
        sf::Vector2f slotCenter(int idx) const {
            float slotW = m_width * 0.6f / SELECTION_SIZE;
            float x = m_origin.x + slotW * (idx + 0.5f);
            float y = m_origin.y + BAR_HEIGHT * 0.5f;
            return {x, y};
        }

        // Hit test: fare hangi slot üzerinde?
        int hitTest(sf::Vector2f pos) const {
            if (pos.y < m_origin.y || pos.y > m_origin.y + BAR_HEIGHT) return -1;
            float slotW = m_width * 0.6f / SELECTION_SIZE;
            for (int i = 0; i < SELECTION_SIZE; ++i) {
                float x0 = m_origin.x + slotW * i;
                float x1 = x0 + slotW;
                if (pos.x >= x0 && pos.x < x1) return i;
            }
            return -1;
        }

        void drawPiecePreview(sf::RenderWindow &window, const Piece &piece,
                              int slotIdx, float alpha) {
            sf::Vector2f center = slotCenter(slotIdx);
            // Parçanın bounding box'ını hesapla (local)
            int minR = 4, maxR = -1, minC = 4, maxC = -1;
            for (auto [r, c]: piece.localCells()) {
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
            if (maxR < 0) return;

            float pW = static_cast<float>(maxC - minC + 1) * PIECE_PREVIEW_CELL;
            float pH = static_cast<float>(maxR - minR + 1) * PIECE_PREVIEW_CELL;
            float startX = center.x - pW * 0.5f;
            float startY = center.y - pH * 0.5f;

            sf::RectangleShape cell({PIECE_PREVIEW_CELL - 2.f, PIECE_PREVIEW_CELL - 2.f});
            sf::Color c = piece.color();
            c.a = static_cast<std::uint8_t>(alpha * 255);
            cell.setFillColor(c);
            cell.setOutlineColor(sf::Color(
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.r) + 60)),
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.g) + 60)),
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.b) + 60)), c.a));
            cell.setOutlineThickness(-1.5f);

            for (auto [r, col]: piece.localCells()) {
                float cx = startX + static_cast<float>(col - minC) * PIECE_PREVIEW_CELL + 1.f;
                float cy = startY + static_cast<float>(r - minR) * PIECE_PREVIEW_CELL + 1.f;
                cell.setPosition({cx, cy});
                window.draw(cell);
            }
        }

        void drawDraggingPiece(sf::RenderWindow &window) {
            const Piece &piece = m_drag.piece;
            int minR = 4, maxR = -1, minC = 4, maxC = -1;
            for (auto [r, c]: piece.localCells()) {
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
            if (maxR < 0) return;

            float pW = static_cast<float>(maxC - minC + 1) * CELL_SIZE;
            float pH = static_cast<float>(maxR - minR + 1) * CELL_SIZE;
            float startX = m_drag.mousePos.x - pW * 0.5f;
            float startY = m_drag.mousePos.y - pH * 0.5f;

            sf::RectangleShape cell({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
            sf::Color c = piece.color();
            c.a = 200;
            cell.setFillColor(c);
            cell.setOutlineColor(sf::Color(
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.r) + 60)),
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.g) + 60)),
                static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.b) + 60)), c.a));
            cell.setOutlineThickness(-2.f);

            for (auto [r, col]: piece.localCells()) {
                float cx = startX + static_cast<float>(col - minC) * CELL_SIZE + 1.f;
                float cy = startY + static_cast<float>(r - minR) * CELL_SIZE + 1.f;
                cell.setPosition({cx, cy});
                window.draw(cell);
            }
        }

        void drawSlotBorder(sf::RenderWindow &window, int idx) {
            float slotW = m_width * 0.6f / SELECTION_SIZE;
            sf::RectangleShape border({slotW - 6.f, BAR_HEIGHT - 12.f});
            border.setPosition({m_origin.x + slotW * idx + 3.f, m_origin.y + 6.f});
            border.setFillColor(sf::Color::Transparent);
            bool dragging = m_drag.active && m_drag.selectionIndex == idx;
            border.setOutlineColor(dragging
                                       ? sf::Color(255, 255, 100, 200)
                                       : sf::Color(60, 60, 100, 160));
            border.setOutlineThickness(1.5f);
            window.draw(border);
        }

        void drawText(sf::RenderWindow &window, const std::string &str,
                      sf::Vector2f pos, unsigned size,
                      sf::Color color = sf::Color(200, 200, 240)) {
            if (!m_font) return;

            sf::Text text(*m_font);
            text.setString(sf::String::fromUtf8(str.begin(), str.end()));
            text.setCharacterSize(size);
            text.setFillColor(color);

            auto bounds = text.getLocalBounds();
            text.setOrigin({bounds.size.x * 0.5f, bounds.size.y * 0.5f});
            text.setPosition(pos);
            window.draw(text);
        }

        sf::Vector2f m_origin;
        float m_width;
        sf::RectangleShape m_background;
        DragState m_drag;
        const sf::Font *m_font{nullptr};
    };
} // namespace tetris
