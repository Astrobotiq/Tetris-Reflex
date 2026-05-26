#pragma once

#include "GameState.hpp"
#include "TetrisConstants.hpp"
#include "ColorPalette.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace tetris {

    class TetrisHUD {
    public:
        void setFont(const sf::Font& f) { m_font = &f; }

        void update(float dt) {
            m_scoreScale.x += (1.f - m_scoreScale.x) * 12.f * dt;
            m_scoreScale.y += (1.f - m_scoreScale.y) * 12.f * dt;
            m_levelScale.x += (1.f - m_levelScale.x) * 12.f * dt;
            m_levelScale.y += (1.f - m_levelScale.y) * 12.f * dt;
            m_linesScale.x += (1.f - m_linesScale.x) * 12.f * dt;
            m_linesScale.y += (1.f - m_linesScale.y) * 12.f * dt;
            m_tokensScale.x += (1.f - m_tokensScale.x) * 12.f * dt;
            m_tokensScale.y += (1.f - m_tokensScale.y) * 12.f * dt;
        }

        void renderSidePanel(sf::RenderWindow& window, const GameState& gameState, const ColorPalette& pal) {
            float px = BOARD_X + BOARD_PIXEL_W + 10.f, py = BOARD_Y;
            float pw = float(WIN_W) - px - 5.f;

            // Trigger animations on value change
            if (gameState.score != m_prevScore) {
                if (m_prevScore != -1) m_scoreScale = {1.5f, 0.6f};
                m_prevScore = gameState.score;
            }
            if (gameState.level != m_prevLevel) {
                if (m_prevLevel != -1) m_levelScale = {1.5f, 0.6f};
                m_prevLevel = gameState.level;
            }
            if (gameState.linesCleared != m_prevLines) {
                if (m_prevLines != -1) m_linesScale = {1.5f, 0.6f};
                m_prevLines = gameState.linesCleared;
            }
            if (gameState.slotMachine.tokens != m_prevTokens) {
                if (m_prevTokens != -1) m_tokensScale = {1.5f, 0.6f};
                m_prevTokens = gameState.slotMachine.tokens;
            }

            sf::RectangleShape panel(sf::Vector2f{pw, BOARD_PIXEL_H});
            panel.setPosition(sf::Vector2f{px, py});
            panel.setFillColor(pal.panelBg);
            panel.setOutlineColor(pal.panelBorder);
            panel.setOutlineThickness(1.5f);
            window.draw(panel);

            // Neon Border Glow
            for (int i = 0; i < 3; ++i) {
                sf::RectangleShape glow(sf::Vector2f(pw - i * 2.f, BOARD_PIXEL_H - i * 2.f));
                glow.setPosition(sf::Vector2f(px + static_cast<float>(i), py + static_cast<float>(i)));
                glow.setFillColor(sf::Color::Transparent);
                sf::Color outlineCol = pal.uiAccent;
                outlineCol.a = static_cast<std::uint8_t>(40 + i * 40);
                glow.setOutlineColor(outlineCol);
                glow.setOutlineThickness(1.f);
                window.draw(glow);
            }

            if (!m_font) return;

            txt(window, "NEXT", sf::Vector2f{px + 8, py + 8}, 13, sf::Color(160, 160, 220));
            for (int i = 0; i < 3; ++i)
                drawMiniPiece(window, gameState.nextPieces[i], sf::Vector2f{px + 12, py + 28.f + i * 60}, 16);

            float iy = py + 220;
            txt(window, "SCORE", sf::Vector2f{px + 8, iy}, 12, sf::Color(140, 140, 200));
            txt(window, std::to_string(gameState.score), sf::Vector2f{px + 8, iy + 16}, 16, sf::Color(255, 255, 100), m_scoreScale);
            
            txt(window, "LEVEL", sf::Vector2f{px + 8, iy + 44}, 12, sf::Color(140, 140, 200));
            txt(window, std::to_string(gameState.level), sf::Vector2f{px + 8, iy + 60}, 16, sf::Color(100, 255, 180), m_levelScale);
            
            // Level progress bar under Level
            float barY = iy + 80.f;
            float barW = pw - 20.f;
            float barH = 5.f;
            sf::RectangleShape barBg(sf::Vector2f{barW, barH});
            barBg.setPosition(sf::Vector2f{px + 8.f, barY});
            barBg.setFillColor(sf::Color(30, 30, 50));
            window.draw(barBg);

            float progress = (gameState.linesCleared % 10) / 10.f;
            if (progress > 0.f) {
                sf::RectangleShape barFill(sf::Vector2f{barW * progress, barH});
                barFill.setPosition(sf::Vector2f{px + 8.f, barY});
                barFill.setFillColor(pal.uiAccent);
                window.draw(barFill);
            }

            txt(window, "LINES", sf::Vector2f{px + 8, iy + 92}, 12, sf::Color(140, 140, 200));
            txt(window, std::to_string(gameState.linesCleared), sf::Vector2f{px + 8, iy + 108}, 16, sf::Color(180, 220, 255), m_linesScale);

            float jy = iy + 144;
            txt(window, "TOKENS", sf::Vector2f{px + 8, jy}, 12, sf::Color(220, 200, 80));
            txt(window, std::to_string(gameState.slotMachine.tokens), sf::Vector2f{px + 8, jy + 16}, 18, sf::Color(255, 220, 50), m_tokensScale);

            if (gameState.levelTransitionActive) {
                float ly = jy + 52.f;
                int secsLeft = static_cast<int>(std::ceil(gameState.levelTransitionTimer));
                float pulse = 0.5f + 0.5f * std::sin(gameState.levelTransitionTimer * 8.f);

                sf::RectangleShape box(sf::Vector2f{pw - 8.f, 52.f});
                box.setPosition(sf::Vector2f{px + 4.f, ly - 2.f});
                box.setFillColor(sf::Color(
                    static_cast<std::uint8_t>(40 + 60 * pulse), 20,
                    static_cast<std::uint8_t>(60 + 80 * pulse), 200));
                box.setOutlineColor(sf::Color(
                    static_cast<std::uint8_t>(180 + 75 * pulse), 100, 255, 220));
                box.setOutlineThickness(1.5f);
                window.draw(box);

                txt(window, "COOLDOWN", sf::Vector2f{px + 8, ly + 2.f}, 10, sf::Color(200, 160, 255));
                txt(window, std::to_string(secsLeft) + "s", sf::Vector2f{px + 8, ly + 18.f}, 18,
                    sf::Color(static_cast<std::uint8_t>(200 + 55 * pulse), 180, 255));
            }
        }

        void renderLeftPanel(sf::RenderWindow& window, const GameState& gameState, const ColorPalette& pal, bool powerSelectMode, int hoveredPowerIdx) {
            sf::RectangleShape panel(sf::Vector2f{LEFT_PANEL_W, BOARD_PIXEL_H});
            panel.setPosition(sf::Vector2f{0.f, BOARD_Y});
            panel.setFillColor(pal.panelBg);
            panel.setOutlineColor(pal.panelBorder);
            panel.setOutlineThickness(1.5f);
            window.draw(panel);

            // Neon Border Glow
            for (int i = 0; i < 3; ++i) {
                sf::RectangleShape glow(sf::Vector2f(LEFT_PANEL_W - i * 2.f, BOARD_PIXEL_H - i * 2.f));
                glow.setPosition(sf::Vector2f(static_cast<float>(i), BOARD_Y + static_cast<float>(i)));
                glow.setFillColor(sf::Color::Transparent);
                sf::Color outlineCol = pal.uiAccent;
                outlineCol.a = static_cast<std::uint8_t>(40 + i * 40);
                glow.setOutlineColor(outlineCol);
                glow.setOutlineThickness(1.f);
                window.draw(glow);
            }

            if (!m_font) return;

            txt(window, "POWERS", sf::Vector2f{6.f, BOARD_Y + 8.f}, 13, sf::Color(180, 180, 255));

            const auto& powers = gameState.slotMachine.powers;

            if (powers.empty()) {
                txt(window, "(empty)", sf::Vector2f{10.f, BOARD_Y + 30.f}, 11, sf::Color(80, 80, 100));
            }

            for (int i = 0; i < static_cast<int>(powers.size()); ++i) {
                float y = BOARD_Y + 28.f + i * 44.f;

                sf::RectangleShape btn(sf::Vector2f{LEFT_PANEL_W - 8.f, 38.f});
                btn.setPosition(sf::Vector2f{4.f, y});

                bool hovered = hoveredPowerIdx == i;
                sf::Color c = powers[i].color();
                btn.setFillColor(sf::Color(c.r / 4, c.g / 4, c.b / 4, hovered ? 220 : 160));
                btn.setOutlineColor(hovered ? c : sf::Color(c.r / 2, c.g / 2, c.b / 2, 160));
                btn.setOutlineThickness(hovered ? 2.f : 1.f);
                window.draw(btn);

                // Draw hotkey tag circle/box on the left side of the button
                sf::RectangleShape tag(sf::Vector2f{16.f, 16.f});
                tag.setPosition(sf::Vector2f{8.f, y + 11.f});
                tag.setFillColor(sf::Color(15, 15, 25));
                tag.setOutlineColor(sf::Color(c.r, c.g, c.b, 160));
                tag.setOutlineThickness(1.f);
                window.draw(tag);

                txt(window, std::to_string(i + 1), sf::Vector2f{13.f, y + 12.f}, 9, sf::Color(c.r, c.g, c.b, 220));

                txt(window, powers[i].name(), sf::Vector2f{30.f, y + 4.f}, 10, sf::Color(c.r, c.g, c.b, 220));
                if (powerSelectMode)
                    txt(window, "click or key!", sf::Vector2f{30.f, y + 20.f}, 9, sf::Color(200, 200, 100));
                else
                    txt(window, "Press [" + std::to_string(i + 1) + "]", sf::Vector2f{30.f, y + 20.f}, 9, sf::Color(140, 140, 140));
            }

            // Draw a beautiful tooltip card next to the left panel if a power is hovered
            if (hoveredPowerIdx >= 0 && hoveredPowerIdx < static_cast<int>(powers.size())) {
                float tx = LEFT_PANEL_W + 10.f;
                float ty = BOARD_Y + 28.f + hoveredPowerIdx * 44.f;
                float tw = 230.f;
                float th = 64.f;

                // Make sure tooltip stays within screen height
                if (ty + th > BOARD_Y + BOARD_PIXEL_H) {
                    ty = BOARD_Y + BOARD_PIXEL_H - th - 5.f;
                }

                sf::RectangleShape tooltip(sf::Vector2f{tw, th});
                tooltip.setPosition(sf::Vector2f{tx, ty});
                tooltip.setFillColor(sf::Color(20, 20, 30, 245));
                
                sf::Color c = powers[hoveredPowerIdx].color();
                tooltip.setOutlineColor(c);
                tooltip.setOutlineThickness(1.5f);
                window.draw(tooltip);

                // Tooltip Neon Outer Glow
                for (int j = 0; j < 2; ++j) {
                    sf::RectangleShape glow(sf::Vector2f{tw + j * 2.f, th + j * 2.f});
                    glow.setPosition(sf::Vector2f{tx - j, ty - j});
                    glow.setFillColor(sf::Color::Transparent);
                    sf::Color gc = c;
                    gc.a = static_cast<std::uint8_t>(40 + j * 30);
                    glow.setOutlineColor(gc);
                    glow.setOutlineThickness(1.f);
                    window.draw(glow);
                }

                txt(window, powers[hoveredPowerIdx].name(), sf::Vector2f{tx + 8.f, ty + 6.f}, 11, c);
                txt(window, powers[hoveredPowerIdx].description(), sf::Vector2f{tx + 8.f, ty + 24.f}, 9, sf::Color(200, 200, 220));
                txt(window, "Press [" + std::to_string(hoveredPowerIdx + 1) + "] to use directly", sf::Vector2f{tx + 8.f, ty + 44.f}, 8, sf::Color(130, 130, 150));
            }

            if (!powers.empty()) {
                float btnY = BOARD_Y + BOARD_PIXEL_H - 36.f;
                sf::RectangleShape toggleBtn(sf::Vector2f{LEFT_PANEL_W - 8.f, 30.f});
                toggleBtn.setPosition(sf::Vector2f{4.f, btnY});
                toggleBtn.setFillColor(powerSelectMode ? sf::Color(80, 180, 80, 200) : sf::Color(40, 80, 40, 160));
                toggleBtn.setOutlineColor(sf::Color(100, 200, 100, 200));
                toggleBtn.setOutlineThickness(1.5f);
                window.draw(toggleBtn);
                txt(window, powerSelectMode ? "CANCEL" : "USE POWER", sf::Vector2f{8.f, btnY + 7.f}, 11, sf::Color::White);
            }
        }

        void renderTokenMilestoneNotif(sf::RenderWindow& window, float timer) {
            if (!m_font) return;
            float t = timer / 2.0f;
            float alpha = t < 0.2f ? (t / 0.2f) : (t > 0.8f ? ((1.f - t) / 0.2f) : 1.f);
            std::uint8_t a = static_cast<std::uint8_t>(alpha * 230);

            float bw = 210.f, bh = 44.f;
            float bx = BOARD_X + (BOARD_PIXEL_W - bw) * 0.5f;
            float by = BOARD_Y + 12.f;
            sf::RectangleShape box(sf::Vector2f{bw, bh});
            box.setPosition(sf::Vector2f{bx, by});
            box.setFillColor(sf::Color(30, 20, 50, a));
            box.setOutlineColor(sf::Color(220, 180, 60, a));
            box.setOutlineThickness(2.f);
            window.draw(box);

            txt(window, "+1 TOKEN", sf::Vector2f{bx + 60.f, by + 4.f}, 15, sf::Color(255, 220, 50, a));
            txt(window, "1000 point milestone!", sf::Vector2f{bx + 40.f, by + 24.f}, 11, sf::Color(200, 170, 255, a));
        }

        void renderSlotResult(sf::RenderWindow& window, const SlotResult& result, float timer) {
            float alpha = std::min(timer / 0.5f, 1.f);
            if (timer < 0.5f) alpha = timer / 0.5f;

            sf::RectangleShape banner(sf::Vector2f{300.f, 60.f});
            banner.setPosition(sf::Vector2f{BOARD_X + (BOARD_PIXEL_W - 300.f) * 0.5f, BOARD_Y + BOARD_PIXEL_H * 0.3f});
            banner.setFillColor(sf::Color(20, 20, 40, static_cast<std::uint8_t>(200 * alpha)));
            banner.setOutlineColor(sf::Color(200, 200, 80, static_cast<std::uint8_t>(255 * alpha)));
            banner.setOutlineThickness(2.f);
            window.draw(banner);

            if (!m_font) return;

            std::string comboText = (result.comboCount >= 3) ? "3x COMBO!" : "2x COMBO!";
            PowerSlot tempSlot; tempSlot.type = result.power;

            txt(window, comboText, sf::Vector2f{banner.getPosition().x + 80.f, banner.getPosition().y + 6.f}, 20, sf::Color(255, 220, 50, static_cast<std::uint8_t>(255 * alpha)));
            txt(window, tempSlot.name(), sf::Vector2f{banner.getPosition().x + 80.f, banner.getPosition().y + 34.f}, 13, sf::Color(200, 200, 255, static_cast<std::uint8_t>(220 * alpha)));
        }

        void renderOPlacementHint(sf::RenderWindow& window) {
            if (!m_font) return;
            sf::RectangleShape overlay(sf::Vector2f{float(BOARD_PIXEL_W), 40.f});
            overlay.setPosition(sf::Vector2f{BOARD_X, BOARD_Y - 44.f});
            overlay.setFillColor(sf::Color(60, 20, 100, 200));
            overlay.setOutlineColor(sf::Color(180, 80, 255, 220));
            overlay.setOutlineThickness(1.5f);
            window.draw(overlay);

            txt(window, "Click the column where you want to drop the piece!", sf::Vector2f{BOARD_X + 10.f, BOARD_Y - 40.f}, 12, sf::Color(220, 180, 255));
        }

        void renderOverlay(sf::RenderWindow& window, const std::string& title, const std::string& sub) {
            sf::RectangleShape dim(sf::Vector2f{float(WIN_W), float(WIN_H)});
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(dim);
            
            if (!m_font) return;
            txt(window, title, sf::Vector2f{(WIN_W / 2.f) - 100.f, WIN_H * 0.35f}, 38, sf::Color(255, 220, 60));
            txt(window, sub, sf::Vector2f{(WIN_W / 2.f) - 100.f, WIN_H * 0.35f + 50.f}, 18, sf::Color(200, 200, 255));
        }

    private:
        const sf::Font* m_font{nullptr};
        int m_prevScore{-1};
        int m_prevLevel{-1};
        int m_prevLines{-1};
        int m_prevTokens{-1};

        sf::Vector2f m_scoreScale{1.f, 1.f};
        sf::Vector2f m_levelScale{1.f, 1.f};
        sf::Vector2f m_linesScale{1.f, 1.f};
        sf::Vector2f m_tokensScale{1.f, 1.f};

        void txt(sf::RenderWindow& window, const std::string& s, sf::Vector2f pos, unsigned sz, sf::Color col, sf::Vector2f scale = {1.f, 1.f}) {
            sf::Text t(*m_font, s, sz);
            t.setFillColor(col);
            t.setScale(scale);
            if (scale.x != 1.f || scale.y != 1.f) {
                sf::FloatRect b = t.getLocalBounds();
                t.setOrigin({0.f, b.size.y * 0.5f});
                pos.y += sz * 0.5f * (scale.y - 1.f);
            }
            t.setPosition(pos);
            window.draw(t);
        }

        void drawMiniPiece(sf::RenderWindow& window, const Piece& piece, sf::Vector2f origin, float cellSize) {
            sf::RectangleShape cell(sf::Vector2f{cellSize - 2.f, cellSize - 2.f});
            cell.setFillColor(piece.color());
            sf::Color bright = piece.color();
            bright.r = static_cast<std::uint8_t>(std::min(255, int(bright.r) + 60));
            bright.g = static_cast<std::uint8_t>(std::min(255, int(bright.g) + 60));
            bright.b = static_cast<std::uint8_t>(std::min(255, int(bright.b) + 60));
            cell.setOutlineColor(bright);
            cell.setOutlineThickness(-1.5f);

            int minR = 4, maxR = -1, minC = 4, maxC = -1;
            for (auto [r, c] : piece.localCells()) {
                minR = std::min(minR, r); maxR = std::max(maxR, r);
                minC = std::min(minC, c); maxC = std::max(maxC, c);
            }
            if (maxR < 0) return;

            for (auto [r, c] : piece.localCells()) {
                cell.setPosition(sf::Vector2f{origin.x + (c - minC) * cellSize + 1.f, origin.y + (r - minR) * cellSize + 1.f});
                window.draw(cell);
            }
        }
    };
} // namespace tetris