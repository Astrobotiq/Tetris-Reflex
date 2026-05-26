#include "MainMenuScene.hpp"
#include "tetris/TetrisScene.hpp"
#include "tetris/TetrisConstants.hpp"
#include "tetris/ColorPalette.hpp"
#include "../WindowUtils.hpp"
#include <iostream>

namespace tetris {
    MainMenuScene::MainMenuScene(sf::RenderWindow &win, AppSettings *appSettings)
        : window(win), settings(appSettings) {
        setupLayout();
        btnScales.assign(9, 1.0f);
    }

    void MainMenuScene::setupLayout() {
        float cx = 700.f / 2.f;

        // Palet butonları 2x2 grid (Pause menüsündeki gibi geniş)
        paletteBtns.clear();
        float startX = cx - 120.f;
        for(int i = 0; i < 4; ++i) {
            int col = i % 2;
            int row = i / 2;
            paletteBtns.push_back(sf::FloatRect({startX + col * 130.f, 170.f + row * 50.f}, {110.f, 40.f}));
        }

        // Butonları palet gridinin altına ittirdik
        btnSfx        = sf::FloatRect({cx - 120.f, 290.f}, {110.f, 40.f});
        btnMusic      = sf::FloatRect({cx + 10.f,  290.f}, {110.f, 40.f});

        btnNewGame    = sf::FloatRect({cx - 100.f, 360.f}, {200.f, 50.f});
        btnHighScores = sf::FloatRect({cx - 100.f, 430.f}, {200.f, 50.f});
        btnQuit       = sf::FloatRect({cx - 100.f, 500.f}, {200.f, 50.f});

        btnBack = sf::FloatRect({cx - 75.f, 600.f}, {150.f, 40.f});
    }

    void MainMenuScene::onEnter() {
        if (!font.openFromFile("assets/font.ttf")) {
            std::cerr << "[MainMenu] Font yuklenemedi!\n";
        } else {
            fontLoaded = true;
        }

        // Skorları dosyadan yükle
        highScores = ScoreManager::load("assets/scores.csv");
    }

    void MainMenuScene::onExit() {
    }

    static float easeOutBack(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.f;
        float tMinus1 = t - 1.f;
        return 1.f + c3 * std::pow(tMinus1, 3.f) + c1 * std::pow(tMinus1, 2.f);
    }

    void MainMenuScene::handleEvent(const sf::Event &event) {
        sf::Vector2f mouse = sf::Vector2f(sf::Mouse::getPosition(window));
        // Viewport koordinatlarına çevir
        mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (showScoreTable) {
            if (event.is<sf::Event::MouseMoved>()) {
                backHovered = btnBack.contains(mouse);
            } else if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (e->button == sf::Mouse::Button::Left && btnBack.contains(mouse)) {
                    showScoreTable = false;
                    m_introTimer = 0.f; // Reset intro so buttons slide back in!
                }
            }
            return; // Overlay açıksa ana menü eventlerini yut
        }

        if (event.is<sf::Event::MouseMoved>()) {
            hoveredBtn = -1;
            if (btnNewGame.contains(mouse)) hoveredBtn = 0;
            else if (btnHighScores.contains(mouse)) hoveredBtn = 1;
            else if (btnQuit.contains(mouse)) hoveredBtn = 2;
            else if (btnSfx.contains(mouse)) hoveredBtn = 3;
            else if (btnMusic.contains(mouse)) hoveredBtn = 4;
            else {
                for (int i = 0; i < 4; ++i) {
                    if (paletteBtns[i].contains(mouse)) hoveredBtn = 5 + i;
                }
            }

            // Option B: Hover Spark Trails
            if (hoveredBtn != -1) {
                const auto& pal = PALETTES[settings->activePalette];
                for (int p = 0; p < 2; ++p) {
                    float vx = ((std::rand() % 100) - 50) * 1.5f;
                    float vy = ((std::rand() % 100) - 50) * 1.5f;
                    float lifetime = 0.3f + (std::rand() % 30) / 100.f;
                    float size = 2.f + (std::rand() % 3);
                    sf::Color pColor = pal.uiAccent;
                    pColor.a = 200;
                    m_particles.push_back({mouse, {vx, vy}, pColor, 0.f, lifetime, size});
                }
            }
        } else if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (e->button == sf::Mouse::Button::Left) {
                if (btnNewGame.contains(mouse)) {
                    m_clickedBtn = 0;
                    m_clickTimer = 0.f;
                    m_sceneManager->replace(std::make_unique<tetris::TetrisScene>(window, settings));
                } else if (btnHighScores.contains(mouse)) {
                    m_clickedBtn = 1;
                    m_clickTimer = 0.f;
                } else if (btnQuit.contains(mouse)) {
                    m_clickedBtn = 2;
                    m_clickTimer = 0.f;
                } else if (btnSfx.contains(mouse)) {
                    m_clickedBtn = 3;
                    m_clickTimer = 0.f;
                    settings->sfxEnabled = !settings->sfxEnabled;
                } else if (btnMusic.contains(mouse)) {
                    m_clickedBtn = 4;
                    m_clickTimer = 0.f;
                    settings->musicEnabled = !settings->musicEnabled;
                } else {
                    for (int i = 0; i < 4; ++i) {
                        if (paletteBtns[i].contains(mouse)) {
                            m_clickedBtn = 5 + i;
                            m_clickTimer = 0.f;
                            settings->activePalette = i;
                        }
                    }
                }
            }
        }
    }

    void MainMenuScene::update(float dt, engine::InputManager &/*input*/) {
        m_introTimer += dt;
        m_elapsedTime += dt;
        if (showScoreTable) {
            m_scoreTableTimer += dt;
        } else {
            m_scoreTableTimer = 0.f;
        }

        // Handle button click delays
        if (m_clickedBtn != -1) {
            m_clickTimer += dt;
            if (m_clickedBtn == 1 && m_clickTimer >= 0.2f && !showScoreTable) {
                showScoreTable = true;
                m_scoreTableTimer = 0.f;
                m_clickedBtn = -1;
            }
            if (m_clickedBtn == 2 && m_clickTimer >= 0.25f) {
                window.close();
            }
            // Reset state toggles click anim after a short time
            if ((m_clickedBtn == 3 || m_clickedBtn == 4 || m_clickedBtn >= 5) && m_clickTimer >= 0.25f) {
                m_clickedBtn = -1;
            }
        }

        // Option A: Mouse-Reactive Parallax / Tilt
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Vector2f center(700.f * 0.5f, 740.f * 0.5f);
        sf::Vector2f mouseDiff = mouse - center;
        sf::Vector2f targetTilt = { mouseDiff.x * 0.05f, mouseDiff.y * 0.05f };
        m_mouseTilt += (targetTilt - m_mouseTilt) * 10.f * dt;

        for (int i = 0; i < 9; ++i) {
            float target = (hoveredBtn == i) ? 1.08f : 1.0f;
            btnScales[i] += (target - btnScales[i]) * 15.f * dt;
        }

        // Option B: Particle update
        for (auto it = m_particles.begin(); it != m_particles.end(); ) {
            it->position += it->velocity * dt;
            it->age += dt;
            if (it->age >= it->lifetime) {
                it = m_particles.erase(it);
            } else {
                ++it;
            }
        }
    }

    void MainMenuScene::render(sf::RenderWindow &renderWindow, float /*alpha*/) {
        if (!fontLoaded) return;
        updateViewport(renderWindow, 700.f, 740.f);
        const auto &pal = PALETTES[settings->activePalette];

        sf::Color letterboxCol = pal.panelBorder;
        letterboxCol.r /= 2;
        letterboxCol.g /= 2;
        letterboxCol.b /= 2;
        renderWindow.clear(letterboxCol);

        sf::RectangleShape bgRect({700.f, 740.f});
        bgRect.setFillColor(pal.background);
        renderWindow.draw(bgRect);

        sf::Color gridCol = pal.gridLines;
        for (float x = 0; x <= 700.f; x += 35.f) {
            sf::RectangleShape line({1.f, 740.f});
            line.setPosition({x, 0.f});
            line.setFillColor(gridCol);
            renderWindow.draw(line);
        }
        for (float y = 0; y <= 740.f; y += 35.f) {
            sf::RectangleShape line({700.f, 1.f});
            line.setPosition({0.f, y});
            line.setFillColor(gridCol);
            renderWindow.draw(line);
        }

        auto drawText = [&](const std::string &str, float x, float y, int size, sf::Color col, bool center = false, float yOffset = 0.f, float xOffset = 0.f) {
            sf::Text t(font, str, size);
            t.setFillColor(col);
            if (center) {
                sf::FloatRect b = t.getLocalBounds();
                t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
                t.setPosition({x + xOffset, y + yOffset});
            } else {
                t.setPosition({x + xOffset, y + yOffset});
            }
            renderWindow.draw(t);
        };

        auto drawBtn = [&](sf::FloatRect rect, const std::string &text, int btnIdx) {
            float localTime = m_introTimer - (btnIdx * 0.05f);
            float tVal = std::max(0.f, std::min(1.f, localTime / 0.4f));
            float animProgress = easeOutBack(tVal);

            float yOffset = (1.f - animProgress) * 350.f;
            float scale = btnScales[btnIdx] * animProgress;

            // Apply click feedback
            float clickScale = 1.f;
            float clickWiggle = 0.f;
            if (btnIdx == m_clickedBtn) {
                clickScale = 0.85f + 0.15f * std::min(1.f, m_clickTimer / 0.15f);
                clickWiggle = std::sin(m_clickTimer * 50.f) * 6.f * (1.f - std::min(1.f, m_clickTimer / 0.15f));
            }

            bool isHovered = (hoveredBtn == btnIdx);

            // Option A: Mouse-reactive parallax offset
            sf::Vector2f btnCenter = rect.position + rect.size * 0.5f + sf::Vector2f{0.f, yOffset} + m_mouseTilt * 0.2f;
            float swayAngle = std::sin(m_elapsedTime * 2.5f + btnIdx * 0.5f) * 1.5f + clickWiggle;

            sf::Transform transform;
            transform.translate(btnCenter);
            transform.rotate(sf::degrees(swayAngle));
            transform.scale({scale * clickScale, scale * clickScale});

            sf::RectangleShape rs(rect.size);
            rs.setOrigin(rect.size * 0.5f);
            rs.setPosition({0.f, 0.f});
            rs.setFillColor(isHovered ? pal.uiAccent : pal.panelBorder);
            rs.setOutlineThickness(2.f);
            rs.setOutlineColor(pal.uiAccent);
            renderWindow.draw(rs, transform);

            sf::Color txtCol = isHovered ? pal.background : sf::Color::White;

            std::string btnText = text;
            if (isHovered && btnIdx <= 2) {
                btnText = ">  " + text + "  <";
                txtCol = pal.background;
            }

            sf::Text t(font, btnText, 20);
            t.setFillColor(txtCol);
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            t.setPosition({0.f, 0.f});
            renderWindow.draw(t, transform);
        };

        // Title drop-down bounce
        float tTetris = std::max(0.f, std::min(1.f, (m_introTimer + 0.1f) / 0.5f));
        float progressTetris = easeOutBack(tTetris);
        float yOffsetTetris = (1.f - progressTetris) * -200.f;

        float tReflex = std::max(0.f, std::min(1.f, (m_introTimer + 0.05f) / 0.5f));
        float progressReflex = easeOutBack(tReflex);
        float yOffsetReflex = (1.f - progressReflex) * -200.f;

        drawText("TETRIS", 354.f, 54.f, 60, pal.panelBorder, true, yOffsetTetris + m_mouseTilt.y * 1.5f, m_mouseTilt.x * 1.5f);
        drawText("TETRIS", 350.f, 50.f, 60, pal.uiAccent, true, yOffsetTetris + m_mouseTilt.y * 1.5f, m_mouseTilt.x * 1.5f);
        drawText("REFLEX", 350.f, 100.f, 40, pal.pieceI, true, yOffsetReflex + m_mouseTilt.y * 0.8f, m_mouseTilt.x * 0.8f);

        // COLOR PALETTE Header Text
        float tPalText = std::max(0.f, std::min(1.f, (m_introTimer - 0.2f) / 0.4f));
        float progressPalText = easeOutBack(tPalText);
        float yOffsetPalText = (1.f - progressPalText) * 150.f;
        sf::Color palTextCol = sf::Color::White;
        palTextCol.a = static_cast<std::uint8_t>(255 * tPalText);
        drawText("COLOR PALETTE", 350.f, 150.f, 16, palTextCol, true, yOffsetPalText + m_mouseTilt.y * 0.5f, m_mouseTilt.x * 0.5f);

        // YENİ: Detaylı Palet Çizimi (Balatro-style transforms)
        for (int i = 0; i < 4; ++i) {
            int btnIdx = 5 + i;
            const auto &tp = PALETTES[i];
            bool isActive = (settings->activePalette == i);
            bool isHovered = (hoveredBtn == btnIdx);

            float localTime = m_introTimer - (btnIdx * 0.05f);
            float tVal = std::max(0.f, std::min(1.f, localTime / 0.4f));
            float animProgress = easeOutBack(tVal);

            float yOffset = (1.f - animProgress) * 350.f;
            float scale = btnScales[btnIdx] * animProgress;

            // Apply click feedback
            float clickScale = 1.f;
            float clickWiggle = 0.f;
            if (btnIdx == m_clickedBtn) {
                clickScale = 0.85f + 0.15f * std::min(1.f, m_clickTimer / 0.15f);
                clickWiggle = std::sin(m_clickTimer * 50.f) * 6.f * (1.f - std::min(1.f, m_clickTimer / 0.15f));
            }

            sf::Vector2f btnCenter = paletteBtns[i].position + paletteBtns[i].size * 0.5f + sf::Vector2f{0.f, yOffset} + m_mouseTilt * 0.2f;
            float swayAngle = std::sin(m_elapsedTime * 2.5f + btnIdx * 0.5f) * 1.5f + clickWiggle;

            sf::Transform transform;
            transform.translate(btnCenter);
            transform.rotate(sf::degrees(swayAngle));
            transform.scale({scale * clickScale, scale * clickScale});

            sf::RectangleShape rs(paletteBtns[i].size);
            rs.setOrigin(paletteBtns[i].size * 0.5f);
            rs.setPosition({0.f, 0.f});
            rs.setFillColor(isHovered
                                ? sf::Color(static_cast<std::uint8_t>(std::min(255, tp.panelBg.r + 30)),
                                            static_cast<std::uint8_t>(std::min(255, tp.panelBg.g + 30)),
                                            static_cast<std::uint8_t>(std::min(255, tp.panelBg.b + 30)))
                                : tp.panelBg);
            rs.setOutlineThickness(isActive ? 2.f : 1.f);
            rs.setOutlineColor(isActive ? pal.uiAccent : pal.panelBorder);
            renderWindow.draw(rs, transform);

            auto drawScaledStrip = [&](float localX, sf::Color color) {
                sf::RectangleShape strip(sf::Vector2f{10.f, paletteBtns[i].size.y - 4.f});
                strip.setOrigin({5.f, (paletteBtns[i].size.y - 4.f) * 0.5f});
                strip.setPosition({localX, 0.f});
                strip.setFillColor(color);
                renderWindow.draw(strip, transform);
            };

            drawScaledStrip(-40.f, tp.background);
            drawScaledStrip(-28.f, tp.pieceI);
            drawScaledStrip(-16.f, tp.pieceT);

            // Palet İsmi
            sf::Text t(font, tp.name.substr(0, 8) + ".", 12);
            t.setFillColor(tp.uiText);
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            t.setPosition({25.f, 0.f});
            renderWindow.draw(t, transform);
        }

        std::string sfxText = settings->sfxEnabled ? "SFX: ON" : "SFX: OFF";
        std::string musText = settings->musicEnabled ? "MUS: ON" : "MUS: OFF";
        drawBtn(btnSfx, sfxText, 3);
        drawBtn(btnMusic, musText, 4);

        drawBtn(btnNewGame, "NEW GAME", 0);
        drawBtn(btnHighScores, "HIGH SCORES", 1);
        drawBtn(btnQuit, "QUIT", 2);

        // F11 Fade In
        float tHelpText = std::max(0.f, std::min(1.f, (m_introTimer - 0.5f) / 0.4f));
        sf::Color helpCol = pal.uiAccent;
        helpCol.a = static_cast<std::uint8_t>(255 * tHelpText);
        drawText("F11 - Toggle Fullscreen", 350.f, 680.f, 14, helpCol, true);

        // Option B: Render Hover Spark Trails
        for (const auto& p : m_particles) {
            sf::RectangleShape pShape(sf::Vector2f{p.size, p.size});
            pShape.setOrigin({p.size * 0.5f, p.size * 0.5f});
            pShape.setPosition(p.position);
            
            float lifeRatio = 1.f - (p.age / p.lifetime);
            sf::Color c = p.color;
            c.a = static_cast<std::uint8_t>(p.color.a * lifeRatio);
            pShape.setFillColor(c);
            
            renderWindow.draw(pShape);
        }

        if (showScoreTable) {
            float t = std::max(0.f, std::min(1.f, m_scoreTableTimer / 0.4f));
            float animProgress = easeOutBack(t);
            float yOffset = (1.f - animProgress) * -740.f; // slide down from top

            sf::Transform transform;
            transform.translate({0.f, yOffset});
            renderScoreTable(renderWindow, transform);
        }
    }

    void MainMenuScene::renderScoreTable(sf::RenderWindow &renderWindow, const sf::Transform& transform) {
        sf::RectangleShape dim({700.f, 740.f});
        dim.setFillColor(sf::Color(0, 0, 0, 245));
        renderWindow.draw(dim, transform);

        auto drawText = [&](const std::string &str, float x, float y, int size, sf::Color col, bool center = false) {
            sf::Text t(font, str, size);
            t.setFillColor(col);
            if (center) {
                sf::FloatRect b = t.getLocalBounds();
                t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
                t.setPosition({x, y});
            } else {
                t.setPosition({x, y});
            }
            renderWindow.draw(t, transform);
        };

        drawText("HIGH SCORES", 350.f, 100.f, 40, PALETTES[settings->activePalette].uiAccent, true);

        float startY = 190.f;

        float cRank = 150.f, cName = 250.f, cScore = 380.f, cLvl = 480.f, cLines = 560.f;

        drawText("RANK", cRank, startY, 20, sf::Color::Yellow, true);
        drawText("NAME", cName, startY, 20, sf::Color::Yellow, true);
        drawText("SCORE", cScore, startY, 20, sf::Color::Yellow, true);
        drawText("LVL", cLvl, startY, 20, sf::Color::Yellow, true);
        drawText("LINES", cLines, startY, 20, sf::Color::Yellow, true);

        for (size_t i = 0; i < highScores.size(); ++i) {
            float rowY = startY + 50.f + (i * 35.f);

            drawText(std::to_string(i + 1), cRank, rowY, 18, sf::Color::White, true);
            drawText(highScores[i].name, cName, rowY, 18, sf::Color::White, true);
            drawText(std::to_string(highScores[i].score), cScore, rowY, 18, sf::Color::White, true);
            drawText(std::to_string(highScores[i].level), cLvl, rowY, 18, sf::Color::White, true);
            drawText(std::to_string(highScores[i].lines), cLines, rowY, 18, sf::Color::White, true);
        }

        if (highScores.empty()) {
            drawText("No scores saved yet.", 350.f, startY + 80.f, 18, sf::Color(150, 150, 150), true);
        }

        sf::RectangleShape rs(btnBack.size);
        rs.setPosition(btnBack.position);
        rs.setFillColor(backHovered
                            ? PALETTES[settings->activePalette].uiAccent
                            : PALETTES[settings->activePalette].panelBorder);
        rs.setOutlineThickness(2.f);
        rs.setOutlineColor(PALETTES[settings->activePalette].uiAccent);
        renderWindow.draw(rs, transform);

        sf::Color txtCol = backHovered ? PALETTES[settings->activePalette].background : sf::Color::White;
        drawText("BACK", btnBack.position.x + btnBack.size.x / 2.f, btnBack.position.y + btnBack.size.y / 2.f, 20,
                 txtCol, true);
    }
} // namespace tetris//
// Created by e-r-e on 5/20/2026.
//
