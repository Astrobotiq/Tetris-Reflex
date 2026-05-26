#pragma once

#include "ColorPalette.hpp"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <cstdint>

namespace tetris {
    class PauseMenu {
    public:
        struct Layout {
            sf::FloatRect sfxBtn;
            sf::FloatRect musicBtn;
            sf::FloatRect paletteBtns[4];
            sf::FloatRect resumeBtn;
            sf::FloatRect menuBtn; // YENİ: Main Menu Butonu
        };

        Layout layout;

        std::function<void()> onToggleSFX;
        std::function<void()> onToggleMusic;
        std::function<void(int)> onSelectPalette;
        std::function<void()> onResume;
        std::function<void()> onMainMenu; // YENİ: Main Menu Callback

        int hoveredBtnIndex = -1;

        // UI Animation Timers
        float m_introTimer{0.f};
        float m_elapsedTime{0.f};
        bool m_wasPaused{false};

        int m_clickedBtnIndex{-1};
        float m_clickTimer{0.f};

        bool m_isOutro{false};
        float m_outroTimer{0.f};
        float m_outroDuration{0.2f};
        std::function<void()> m_outroCallback{nullptr};

        void triggerOutro(std::function<void()> callback) {
            m_isOutro = true;
            m_outroTimer = 0.f;
            m_outroCallback = callback;
        }

        void update(float dt, bool isPaused) {
            if (m_isOutro) {
                m_outroTimer += dt;
                m_elapsedTime += dt;
                if (m_outroTimer >= m_outroDuration) {
                    m_isOutro = false;
                    if (m_outroCallback) {
                        m_outroCallback();
                        m_outroCallback = nullptr;
                    }
                }
                return;
            }

            if (m_clickedBtnIndex != -1) {
                m_clickTimer += dt;
                if (m_clickTimer >= 0.25f) {
                    m_clickedBtnIndex = -1;
                }
            }

            if (isPaused) {
                if (!m_wasPaused) {
                    m_introTimer = 0.f;
                    m_wasPaused = true;
                }
                m_introTimer += dt;
                m_elapsedTime += dt;
            } else {
                m_wasPaused = false;
            }
        }

        void updateLayout(float winW, float winH) {
            float cx = winW * 0.5f;
            float cy = winH * 0.5f;

            layout.sfxBtn = sf::FloatRect({cx - 120.f, cy - 100.f}, {110.f, 40.f});
            layout.musicBtn = sf::FloatRect({cx + 10.f, cy - 100.f}, {110.f, 40.f});

            float startX = cx - 120.f;
            float startY = cy - 20.f;
            for (int i = 0; i < 4; ++i) {
                int col = i % 2;
                int row = i / 2;
                layout.paletteBtns[i] = sf::FloatRect({startX + col * 130.f, startY + row * 50.f}, {110.f, 40.f});
            }

            // Butonları alt alta dizdik
            layout.resumeBtn = sf::FloatRect({cx - 80.f, cy + 100.f}, {160.f, 45.f});
            layout.menuBtn = sf::FloatRect({cx - 80.f, cy + 155.f}, {160.f, 45.f});
        }

        void handleMouseMove(sf::Vector2f mouse) {
            if (m_isOutro) return;
            hoveredBtnIndex = -1;
            if (layout.sfxBtn.contains(mouse)) hoveredBtnIndex = 0;
            else if (layout.musicBtn.contains(mouse)) hoveredBtnIndex = 1;
            else if (layout.resumeBtn.contains(mouse)) hoveredBtnIndex = 6;
            else if (layout.menuBtn.contains(mouse)) hoveredBtnIndex = 7; // YENİ
            else {
                for (int i = 0; i < 4; ++i) {
                    if (layout.paletteBtns[i].contains(mouse)) {
                        hoveredBtnIndex = 2 + i;
                        break;
                    }
                }
            }
        }

        bool handleMouseClick(sf::Vector2f mouse) {
            if (m_isOutro) return false;
            if (layout.sfxBtn.contains(mouse)) {
                m_clickedBtnIndex = 0;
                m_clickTimer = 0.f;
                if (onToggleSFX) onToggleSFX();
                return true;
            }
            if (layout.musicBtn.contains(mouse)) {
                m_clickedBtnIndex = 1;
                m_clickTimer = 0.f;
                if (onToggleMusic) onToggleMusic();
                return true;
            }
            if (layout.resumeBtn.contains(mouse)) {
                m_clickedBtnIndex = 6;
                m_clickTimer = 0.f;
                triggerOutro(onResume);
                return true;
            }
            if (layout.menuBtn.contains(mouse)) {
                m_clickedBtnIndex = 7;
                m_clickTimer = 0.f;
                triggerOutro(onMainMenu);
                return true;
            } // YENİ

            for (int i = 0; i < 4; ++i) {
                if (layout.paletteBtns[i].contains(mouse)) {
                    m_clickedBtnIndex = 2 + i;
                    m_clickTimer = 0.f;
                    if (onSelectPalette) onSelectPalette(i);
                    return true;
                }
            }
            return false;
        }

        void render(sf::RenderWindow &window, const sf::Font &font, const ColorPalette &currentPalette, bool sfxOn,
                    bool musicOn, int activePalIndex, float winW, float winH) {
            sf::RectangleShape dim(sf::Vector2f{winW, winH});
            float alphaProgress = std::min(1.f, m_introTimer / 0.25f);
            if (m_isOutro) {
                alphaProgress = 1.f - std::min(1.f, m_outroTimer / m_outroDuration);
            }
            dim.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(170 * alphaProgress)));
            window.draw(dim);

            float cx = winW * 0.5f;
            float cy = winH * 0.5f;

            // Slide down with Outback easing
            float t = std::min(1.f, m_introTimer / 0.35f);
            if (m_isOutro) {
                t = 1.f - std::min(1.f, m_outroTimer / m_outroDuration);
            }
            float animProgress = easeOutBack(t);
            float yOffset = (1.f - animProgress) * -600.f; // slide down/up

            // Gentle wobble tilt
            float swayAngle = std::sin(m_elapsedTime * 2.5f) * 1.2f;

            sf::Transform transform;
            transform.translate({0.f, yOffset});
            transform.rotate(sf::degrees(swayAngle), {cx, cy});

            // Panelin boyunu uzattık (420 -> 460)
            sf::RectangleShape panel(sf::Vector2f{320.f, 460.f});
            panel.setOrigin(sf::Vector2f{160.f, 230.f});
            panel.setPosition(sf::Vector2f{cx, cy});
            panel.setFillColor(currentPalette.boardBg);
            panel.setOutlineColor(currentPalette.uiAccent);
            panel.setOutlineThickness(2.f);
            window.draw(panel, transform);

            drawText(window, font, "PAUSED", sf::Vector2f{cx, cy - 180.f}, 28, currentPalette.uiText, true, transform);

            auto applyClickJuice = [&](sf::Transform& baseTrans, sf::FloatRect btnRect, int btnIdx) {
                if (m_clickedBtnIndex == btnIdx) {
                    float clickScale = 0.85f + 0.15f * std::min(1.f, m_clickTimer / 0.15f);
                    float clickWiggle = std::sin(m_clickTimer * 50.f) * 6.f * (1.f - std::min(1.f, m_clickTimer / 0.15f));
                    sf::Vector2f center = btnRect.position + btnRect.size * 0.5f;
                    baseTrans.translate(center);
                    baseTrans.rotate(sf::degrees(clickWiggle));
                    baseTrans.scale({clickScale, clickScale});
                    baseTrans.translate(-center);
                }
            };

            sf::Transform sfxTrans = transform;
            applyClickJuice(sfxTrans, layout.sfxBtn, 0);
            drawToggleButton(window, font, "SFX", sfxOn, layout.sfxBtn, currentPalette, hoveredBtnIndex == 0, sfxTrans);

            sf::Transform musTrans = transform;
            applyClickJuice(musTrans, layout.musicBtn, 1);
            drawToggleButton(window, font, "MUSIC", musicOn, layout.musicBtn, currentPalette, hoveredBtnIndex == 1, musTrans);

            drawText(window, font, "COLOR PALETTE", sf::Vector2f{cx, cy - 40.f}, 14, currentPalette.uiText, true, transform);

            for (int i = 0; i < 4; ++i) {
                sf::Transform palTrans = transform;
                applyClickJuice(palTrans, layout.paletteBtns[i], 2 + i);
                drawPaletteButton(window, font, PALETTES[i], layout.paletteBtns[i], currentPalette, activePalIndex == i,
                                  hoveredBtnIndex == (2 + i), palTrans);
            }

            // RESUME Butonu
            sf::Transform resTrans = transform;
            applyClickJuice(resTrans, layout.resumeBtn, 6);

            sf::RectangleShape resBtn(sf::Vector2f{layout.resumeBtn.size.x, layout.resumeBtn.size.y});
            resBtn.setPosition(layout.resumeBtn.position);
            resBtn.setFillColor(hoveredBtnIndex == 6 ? lighten(currentPalette.uiAccent, 30) : currentPalette.uiAccent);
            window.draw(resBtn, resTrans);
            sf::Color resTextColor = currentPalette.boardBg;

            // YENİ: Y ekseni için tam ortalama (layout.resumeBtn.size.y * 0.5f)
            drawText(window, font, "RESUME", sf::Vector2f{
                         layout.resumeBtn.position.x + layout.resumeBtn.size.x * 0.5f,
                         layout.resumeBtn.position.y + layout.resumeBtn.size.y * 0.5f
                     }, 18, resTextColor, true, resTrans);

            // MAIN MENU Butonu
            sf::Transform mnuTrans = transform;
            applyClickJuice(mnuTrans, layout.menuBtn, 7);

            sf::RectangleShape mnuBtn(sf::Vector2f{layout.menuBtn.size.x, layout.menuBtn.size.y});
            mnuBtn.setPosition(layout.menuBtn.position);
            mnuBtn.setFillColor(hoveredBtnIndex == 7 ? lighten(currentPalette.uiAccent, 30) : currentPalette.uiAccent);
            mnuBtn.setOutlineThickness(1.f);
            window.draw(mnuBtn, mnuTrans);

            // YENİ: Y ekseni için tam ortalama (layout.menuBtn.size.y * 0.5f)
            drawText(window, font, "MAIN MENU", sf::Vector2f{
                         layout.menuBtn.position.x + layout.menuBtn.size.x * 0.5f,
                         layout.menuBtn.position.y + layout.menuBtn.size.y * 0.5f
                     }, 18, resTextColor, true, mnuTrans);
        }

    private:
        float easeOutBack(float t) const {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.f;
            float tMinus1 = t - 1.f;
            return 1.f + c3 * std::pow(tMinus1, 3.f) + c1 * std::pow(tMinus1, 2.f);
        }

        void drawText(sf::RenderWindow &window, const sf::Font &font, const std::string &str, sf::Vector2f pos,
                      unsigned int size, sf::Color color, bool center, const sf::Transform& transform) {
            sf::Text t(font, str, size);
            t.setFillColor(color);
            t.setStyle(sf::Text::Bold);
            if (center) {
                auto b = t.getLocalBounds();
                t.setOrigin(sf::Vector2f{b.position.x + b.size.x / 2.0f, b.position.y + b.size.y / 2.0f});
            }
            t.setPosition(pos);
            window.draw(t, transform);
        }

        void drawToggleButton(sf::RenderWindow &window, const sf::Font &font, const std::string &label, bool isOn,
                              sf::FloatRect rect, const ColorPalette &p, bool hovered, const sf::Transform& transform) {
            sf::RectangleShape btn(sf::Vector2f{rect.size.x, rect.size.y});
            btn.setPosition(rect.position);
            btn.setFillColor(isOn
                                 ? (hovered ? lighten(p.uiAccent, 30) : p.uiAccent)
                                 : (hovered ? sf::Color(80, 80, 80) : p.panelBg));
            btn.setOutlineColor(p.uiAccent);
            btn.setOutlineThickness(1.f);
            window.draw(btn, transform);
            std::string text = label + (isOn ? ": ON" : ": OFF");
            sf::Color textColor = isOn ? p.boardBg : p.uiText;
            drawText(window, font, text,
                     sf::Vector2f{rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f}, 14,
                     textColor, true, transform);
        }

        void drawPaletteButton(sf::RenderWindow &window, const sf::Font &font, const ColorPalette &targetPal,
                               sf::FloatRect rect, const ColorPalette &currentPal, bool isActive, bool hovered, const sf::Transform& transform) {
            sf::RectangleShape btn(sf::Vector2f{rect.size.x, rect.size.y});
            btn.setPosition(rect.position);
            btn.setFillColor(hovered ? lighten(targetPal.panelBg, 20) : targetPal.panelBg);
            btn.setOutlineColor(isActive ? currentPal.uiAccent : currentPal.panelBorder);
            btn.setOutlineThickness(isActive ? 2.f : 1.f);
            window.draw(btn, transform);

            sf::RectangleShape strip(sf::Vector2f{10.f, rect.size.y - 4.f});
            strip.setPosition(sf::Vector2f{rect.position.x + 2.f, rect.position.y + 2.f});
            strip.setFillColor(targetPal.background);
            window.draw(strip, transform);

            strip.setPosition(sf::Vector2f{rect.position.x + 12.f, rect.position.y + 2.f});
            strip.setFillColor(targetPal.pieceI);
            window.draw(strip, transform);

            strip.setPosition(sf::Vector2f{rect.position.x + 22.f, rect.position.y + 2.f});
            strip.setFillColor(targetPal.pieceT);
            window.draw(strip, transform);

            drawText(window, font, targetPal.name.substr(0, 8) + ".",
                     sf::Vector2f{rect.position.x + 65.f, rect.position.y + rect.size.y * 0.5f}, 12, targetPal.uiText,
                     true, transform);
        }

        static sf::Color lighten(sf::Color c, int amount) {
            return sf::Color(
                static_cast<std::uint8_t>(std::min(255, c.r + amount)),
                static_cast<std::uint8_t>(std::min(255, c.g + amount)),
                static_cast<std::uint8_t>(std::min(255, c.b + amount)),
                c.a);
        }
    };
} // namespace tetris
