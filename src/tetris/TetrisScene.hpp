#pragma once

#include "../SceneManager.hpp"
#include "GameState.hpp"
#include "BoardRenderer.hpp"
#include "SelectionBar.hpp"
#include "ParticleEffect.hpp"
#include "../AudioManager.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <ctime>
#include <memory>
#include <string>

namespace tetris {
    inline constexpr unsigned WIN_W = 700;
    inline constexpr unsigned WIN_H = 740;

    inline constexpr float LEFT_PANEL_W = 140.f;
    inline constexpr float BOARD_X = LEFT_PANEL_W + 10.f;
    inline constexpr float BOARD_Y = 20.f;
    inline constexpr float BOARD_PIXEL_W = BOARD_COLS * CELL_SIZE;
    inline constexpr float BOARD_PIXEL_H = BOARD_ROWS * CELL_SIZE;
    inline constexpr float BAR_Y = BOARD_Y + BOARD_PIXEL_H + 4.f;

    class TetrisScene : public engine::Scene {
    public:
        explicit TetrisScene(sf::RenderWindow &win)
            : window(win)
              , gameState(static_cast<std::uint32_t>(std::time(nullptr)))
              , boardRenderer({BOARD_X, BOARD_Y})
              , selectionBar({0.f, BAR_Y}, float(WIN_W)) {
            setupCallbacks();
        }

        void onEnter() override {
            bool loaded = false;
            for (auto &path: {
                     std::string("assets/font.ttf"),
                     std::string("C:/Windows/Fonts/arial.ttf"),
                     std::string("C:/Windows/Fonts/calibri.ttf"),
                     std::string("C:/Windows/Fonts/segoeui.ttf")
                 }) {
                if (loaded) break;
                if (font.openFromFile(path)) loaded = true;
            }
            if (loaded) {
                selectionBar.setFont(font);
                fontLoaded = true;
            }
            particles.reserve(512);
            powerSelectMode = false;
            selectedPowerIdx = -1;

            // ── Ses efektlerini yükle ──────────────────────────────────────
            audio.loadSound("land", "assets/sounds/piece_land.wav");
            audio.loadSound("clear1", "assets/sounds/line_clear.wav");
            audio.loadSound("clear4", "assets/sounds/line_clear_4.wav");
            audio.loadSound("levelup", "assets/sounds/level_up.wav");
            audio.loadSound("token", "assets/sounds/token_earn.wav");
            audio.loadSound("spin", "assets/sounds/slot_spin.wav");
            audio.loadSound("gameover", "assets/sounds/game_over.wav");
            audio.setVolume(engine::AudioBus::Music, 45.f);
            audio.setVolume(engine::AudioBus::SFX, 85.f);
            audio.playMusic("assets/music/bgm.ogg", true, 0.f);
        }

        void update(float dt, engine::InputManager &input) override {
            if (gameState.gameOver) {
                if (input.isPressed(sf::Keyboard::Key::R)) restartGame();
                audio.update(dt);
                return;
            }

            if (showPowerModal) {
                audio.update(dt);
                selectionBar.update(dt, gameState);
                updateParticles(dt);
                return;
            }

            if (isSpinning) {
                spinWaitTimer -= dt;
                if (spinWaitTimer <= 0.f) {
                    isSpinning = false; // Animasyon bitti!

                    if (lastSlotResult.hasCombo) {
                        showPowerModal = true; // Animasyon bittiği an modalı patlat
                    } else {
                        slotResultTimer = 2.5f; // Kombo yoksa banner çıksın
                        gameState.freezeFalling(false); // Ve oyun düşmeye devam etsin
                    }
                }
            }

            if (input.isPressed(sf::Keyboard::Key::P))
                gameState.paused = !gameState.paused;

            int levelBefore = gameState.level;
            gameState.update(dt);
            selectionBar.update(dt, gameState);

            if (gameState.level > levelBefore)
                audio.playSound("levelup");

            audio.update(dt);

            updateFlash(dt);
            updateParticles(dt);
            updateDragPreview();

            arrowPhase += dt / 1.2f;
            if (arrowPhase > 1.f) arrowPhase -= 1.f;

            if (tokenMilestoneNotifTimer > 0.f) tokenMilestoneNotifTimer -= dt;
            if (slotResultTimer > 0.f) slotResultTimer -= dt;
        }

        void handleEvent(const sf::Event &event) {
            if (gameState.gameOver) return;

            // YENİ: F1 tuşu ile Debug Menüsünü aç/kapat
            if (const auto *e = event.getIf<sf::Event::KeyPressed>()) {
                if (e->code == sf::Keyboard::Key::F1) {
                    showDebugMenu = !showDebugMenu;
                    return;
                }
            }

            sf::Vector2f mouse = sf::Vector2f(sf::Mouse::getPosition(window));

            // YENİ: Debug menüsü açıksa tıklamaları sadece o menü için işle
            if (showDebugMenu) {
                if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
                    if (e->button == sf::Mouse::Button::Left) {
                        float startX = 60.f, startY = 80.f;
                        float btnW = 180.f, btnH = 40.f;
                        float spacingX = 15.f, spacingY = 15.f;

                        // 15 adet gücün konumunu kontrol et (3 sütun, 5 satır)
                        for (int i = 0; i < 15; ++i) {
                            int col = i % 3;
                            int row = i / 3;
                            sf::FloatRect btnRect({startX + col * (btnW + spacingX), startY + row * (btnH + spacingY)},
                                                  {btnW, btnH});

                            if (btnRect.contains(mouse)) {
                                PowerType p = static_cast<PowerType>(i);
                                PowerSlot dummySlot;
                                dummySlot.type = p;

                                // Efekti oynat ve gücü direkt çalıştır!
                                addPowerBlastParticles(dummySlot.color());
                                applyPowerEffect(p);

                                // Menüyü açık bırakıyoruz ki arka arkaya test edebilesin
                                return;
                            }
                        }
                    }
                }
                return; // Debug menüsü açıkken oyun gridine veya barlara tıklanmasını engelle
            }

            // YENİ: Modal açıksa diğer eventleri yut ve kendi butonlarını kontrol et
            if (showPowerModal) {
                if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
                    if (e->button == sf::Mouse::Button::Left) {
                        if (btnUseNowRect.contains(mouse)) {
                            // "ŞİMDİ KULLAN" seçildi
                            showPowerModal = false;
                            gameState.freezeFalling(false);

                            // YENİ: Listeye HİÇ eklemeden direkt havada kullan!
                            addPowerBlastParticles(pendingPower.color());
                            applyPowerEffect(pendingPower.type);
                        } else if (btnKeepRect.contains(mouse)) {
                            // "SAKLA" seçildi
                            showPowerModal = false;
                            gameState.freezeFalling(false);

                            // YENİ: Sadece sakla dediğinde listeye ekle!
                            gameState.slotMachine.awardPower(pendingPower.type);
                        }
                    }
                }
                return; // Modal açıkken arkaya tıklanmasını engelle
            }

            if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (e->button == sf::Mouse::Button::Left) {
                    if (oPlacementMode) {
                        sf::FloatRect br = boardRenderer.boardRect();
                        if (br.contains(mouse)) {
                            int col = boardRenderer.screenXToCol(mouse.x);
                            gameState.applyMoveFallingToCol(col);
                        }
                        gameState.freezeFalling(false);
                        oPlacementMode = false;
                        return;
                    }

                    if (mouse.x < LEFT_PANEL_W && mouse.y < BAR_Y) {
                        if (powerSelectMode) handlePowerClick(mouse);
                        else handleLeftPanelToggle(mouse);
                        return;
                    }

                    if (powerSelectMode) {
                        powerSelectMode = false;
                        gameState.freezeFalling(false);
                        return;
                    }

                    selectionBar.onMousePress(mouse, gameState);
                }
                if (e->button == sf::Mouse::Button::Right)
                    selectionBar.onRightClick(mouse, gameState);
            }

            if (event.is<sf::Event::MouseMoved>())
                selectionBar.onMouseMove(mouse);

            if (const auto *e = event.getIf<sf::Event::MouseButtonReleased>()) {
                if (e->button == sf::Mouse::Button::Left) {
                    auto result = selectionBar.onMouseRelease(
                        mouse, BOARD_X, BOARD_Y, BOARD_Y + BOARD_PIXEL_H, gameState);
                    if (result.success) addPlaceParticles(mouse);
                    boardRenderer.setDragPreview(std::nullopt, true);
                }
            }
        }

        void render(sf::RenderWindow &renderWindow, float) override {
            renderWindow.clear(sf::Color(10, 10, 20));

            boardRenderer.setFlashRows(flashRows,
                                       flashDuration > 0 ? flashTimer / flashDuration : 0.f);
            boardRenderer.render(renderWindow, gameState.board,
                                 gameState.fallingPiece, true);

            if (!gameState.gameOver && !gameState.paused)
                renderNextPieceArrow(renderWindow);

            if (gameState.fallingHasToken)
                drawTokenGlow(renderWindow);

            renderParticles(renderWindow);
            selectionBar.render(renderWindow, gameState);
            renderLeftPanel(renderWindow);
            renderSidePanel(renderWindow);

            if (powerSelectMode) {
                sf::RectangleShape dim(sf::Vector2f{float(BOARD_PIXEL_W), float(BOARD_PIXEL_H)});
                dim.setPosition(sf::Vector2f{BOARD_X, BOARD_Y});
                dim.setFillColor(sf::Color(0, 0, 0, 160));
                renderWindow.draw(dim);

                if (fontLoaded) {
                    sf::Text pt(font, "POWER SELECTION...", 20);
                    pt.setFillColor(sf::Color(100, 255, 100));
                    pt.setStyle(sf::Text::Bold);
                    auto b = pt.getLocalBounds();
                    pt.setPosition(sf::Vector2f{
                        BOARD_X + (BOARD_PIXEL_W - b.size.x) * 0.5f,
                        BOARD_Y + BOARD_PIXEL_H * 0.4f
                    });
                    renderWindow.draw(pt);
                }
            }

            if (oPlacementMode)
                renderOPlacementHint(renderWindow);

            if (slotResultTimer > 0.f)
                renderSlotResult(renderWindow);

            if (tokenMilestoneNotifTimer > 0.f)
                renderTokenMilestoneNotif(renderWindow);

            renderWindow.setView(renderWindow.getDefaultView());
            renderHUD(renderWindow);

            if (showPowerModal) {
                renderPowerModal(renderWindow);
            }

            if (showDebugMenu) {
                renderDebugMenu(renderWindow);
            }

            if (gameState.paused) renderOverlay(renderWindow, "PAUSED", "P: Resume");
            if (showGameOver)
                renderOverlay(renderWindow, "GAME OVER",
                              "Score: " + std::to_string(gameState.score) + "   R: Restart");
        }

    private:
        // ── Callback kurulumu (constructor ve restartGame'de ortak) ───────────────
        void setupCallbacks() {
            gameState.onLinesCleared = [this](int n, std::vector<int> rows) {
                flashRows = rows;
                flashTimer = 0.f;
                flashDuration = 0.35f;
                addLineClearParticles(rows);

                if (n >= 4) audio.playSound("clear4");
                else audio.playSound("clear1");

                int cur = gameState.scoreTokenMilestone;
                if (cur > lastTokenMilestone) {
                    lastTokenMilestone = cur;
                    tokenMilestoneNotifTimer = 2.0f;
                    audio.playSound("token");
                }
            };

            gameState.onPieceLocked = [this]() {
                audio.playSound("land", 0.05f);
            };

            gameState.onGameOver = [this]() {
                showGameOver = true;
                audio.playSound("gameover");
                audio.stopMusic(2.f);
            };

            gameState.onSlotResult = [this](SlotResult result) {
                lastSlotResult = result;
                audio.playSound("spin");

                // Spin animasyonunun süresi (SelectionBar'daki animasyon kaç saniyeyse ona göre ayarla, örn: 1.5 saniye)
                spinWaitTimer = 1.5f;
                isSpinning = true;

                if (result.hasCombo) {
                    pendingPower.type = result.power;
                }

                // Animasyon dönmeye başladığı an düşen parçayı havada dondur
                gameState.freezeFalling(true);
            };
        }

        // ── Sonraki parçanın düşeceği sütun — akan ok göstergesi ─────────────────
        void renderNextPieceArrow(sf::RenderWindow &renderWindow) {
            const Piece &nextP = gameState.nextPieces[0];
            int minC = 4, maxC = -1;
            for (auto [r, c]: nextP.localCells()) {
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
            if (maxC < 0) return;

            int startCol = std::max(0, std::min(gameState.nextPieceCol + minC, BOARD_COLS - 1));
            int endCol = std::max(0, std::min(gameState.nextPieceCol + maxC, BOARD_COLS - 1));

            constexpr int ARROW_COUNT = 3;
            constexpr float ARROW_SPACING = 1.f / ARROW_COUNT;
            constexpr int ARROW_H = 10;
            constexpr int ARROW_W = CELL_SIZE - 6;
            float topY = BOARD_Y - 32.f;

            sf::ConvexShape arrowShape(3);

            for (int col = startCol; col <= endCol; ++col) {
                float cx = BOARD_X + col * CELL_SIZE + CELL_SIZE * 0.5f;

                for (int k = 0; k < ARROW_COUNT; ++k) {
                    float phase = std::fmod(arrowPhase + k * ARROW_SPACING, 1.f);
                    float travel = (BOARD_Y - 4.f) - topY;
                    float y = topY + phase * travel;
                    float fade = std::sin(phase * 3.14159f);
                    std::uint8_t alpha = static_cast<std::uint8_t>(fade * 200.f);

                    sf::Color c = nextP.color();
                    c.a = alpha;

                    arrowShape.setPoint(0, {cx - ARROW_W * 0.5f, y});
                    arrowShape.setPoint(1, {cx + ARROW_W * 0.5f, y});
                    arrowShape.setPoint(2, {cx, y + ARROW_H});
                    arrowShape.setFillColor(c);
                    renderWindow.draw(arrowShape);
                }

                sf::RectangleShape stripe({float(CELL_SIZE), float(BOARD_PIXEL_H)});
                stripe.setPosition({BOARD_X + col * CELL_SIZE, BOARD_Y});
                sf::Color sc = nextP.color();
                sc.a = 18;
                stripe.setFillColor(sc);
                renderWindow.draw(stripe);
            }
        }

        // ── 1000 puan jeton milestone bildirimi ──────────────────────────────────
        void renderTokenMilestoneNotif(sf::RenderWindow &renderWindow) {
            if (!fontLoaded) return;
            float t = tokenMilestoneNotifTimer / 2.0f;
            float alpha = t < 0.2f ? (t / 0.2f) : (t > 0.8f ? ((1.f - t) / 0.2f) : 1.f);
            std::uint8_t a = static_cast<std::uint8_t>(alpha * 230);

            float bw = 210.f, bh = 44.f;
            float bx = BOARD_X + (BOARD_PIXEL_W - bw) * 0.5f;
            float by = BOARD_Y + 12.f;
            sf::RectangleShape box({bw, bh});
            box.setPosition({bx, by});
            box.setFillColor(sf::Color(30, 20, 50, a));
            box.setOutlineColor(sf::Color(220, 180, 60, a));
            box.setOutlineThickness(2.f);
            renderWindow.draw(box);

            sf::Text t1(font, "+1 TOKEN", 15);
            t1.setFillColor(sf::Color(255, 220, 50, a));
            t1.setStyle(sf::Text::Bold);
            auto b1 = t1.getLocalBounds();
            t1.setPosition({bx + (bw - b1.size.x) * 0.5f, by + 4.f});
            renderWindow.draw(t1);

            sf::Text t2(font, "1000 point milestone!", 11);
            t2.setFillColor(sf::Color(200, 170, 255, a));
            auto b2 = t2.getLocalBounds();
            t2.setPosition({bx + (bw - b2.size.x) * 0.5f, by + 24.f});
            renderWindow.draw(t2);
        }

        // ── Sol panel: güçler listesi ─────────────────────────────────────────────
        void renderLeftPanel(sf::RenderWindow &renderWindow) {
            sf::RectangleShape panel(sf::Vector2f{LEFT_PANEL_W, BOARD_PIXEL_H});
            panel.setPosition(sf::Vector2f{0.f, BOARD_Y});
            panel.setFillColor(sf::Color(18, 18, 32));
            panel.setOutlineColor(sf::Color(60, 60, 100));
            panel.setOutlineThickness(1.5f);
            renderWindow.draw(panel);

            if (!fontLoaded) return;

            txt(renderWindow, "POWERS", {6.f, BOARD_Y + 8.f}, 13, sf::Color(180, 180, 255));

            auto &powers = gameState.slotMachine.powers;

            if (powers.empty()) {
                txt(renderWindow, "(empty)", {10.f, BOARD_Y + 30.f}, 11, sf::Color(80, 80, 100));
            }

            for (int i = 0; i < static_cast<int>(powers.size()); ++i) {
                float y = BOARD_Y + 28.f + i * 44.f;

                sf::RectangleShape btn(sf::Vector2f{LEFT_PANEL_W - 8.f, 38.f});
                btn.setPosition(sf::Vector2f{4.f, y});

                bool hovered = powerSelectMode && hoveredPowerIdx == i;
                sf::Color c = powers[i].color();
                btn.setFillColor(sf::Color(c.r / 4, c.g / 4, c.b / 4, hovered ? 220 : 160));
                btn.setOutlineColor(hovered ? c : sf::Color(c.r / 2, c.g / 2, c.b / 2, 160));
                btn.setOutlineThickness(hovered ? 2.f : 1.f);
                renderWindow.draw(btn);

                txt(renderWindow, powers[i].name(), {8.f, y + 4.f}, 10, sf::Color(c.r, c.g, c.b, 220));
                if (powerSelectMode)
                    txt(renderWindow, "click!", {8.f, y + 20.f}, 9, sf::Color(200, 200, 100));
            }

            if (!powers.empty()) {
                float btnY = BOARD_Y + BOARD_PIXEL_H - 36.f;
                sf::RectangleShape toggleBtn(sf::Vector2f{LEFT_PANEL_W - 8.f, 30.f});
                toggleBtn.setPosition(sf::Vector2f{4.f, btnY});
                toggleBtn.setFillColor(powerSelectMode
                                           ? sf::Color(80, 180, 80, 200)
                                           : sf::Color(40, 80, 40, 160));
                toggleBtn.setOutlineColor(sf::Color(100, 200, 100, 200));
                toggleBtn.setOutlineThickness(1.5f);
                renderWindow.draw(toggleBtn);
                txt(renderWindow, powerSelectMode ? "CANCEL" : "USE POWER",
                    {8.f, btnY + 7.f}, 11, sf::Color::White);
            }
        }

        void handlePowerClick(sf::Vector2f mouse) {
            auto &powers = gameState.slotMachine.powers;
            float toggleY = BOARD_Y + BOARD_PIXEL_H - 36.f;

            if (mouse.x < LEFT_PANEL_W && mouse.y >= toggleY && mouse.y < toggleY + 30.f) {
                powerSelectMode = !powerSelectMode;
                gameState.freezeFalling(powerSelectMode);
                return;
            }

            for (int i = 0; i < static_cast<int>(powers.size()); ++i) {
                float y = BOARD_Y + 28.f + i * 44.f;
                if (mouse.x >= 4.f && mouse.x < LEFT_PANEL_W - 4.f &&
                    mouse.y >= y && mouse.y < y + 38.f) {
                    executePower(i, mouse);
                    powerSelectMode = false;
                    if (!oPlacementMode) gameState.freezeFalling(false);
                    return;
                }
            }
        }

        void handleLeftPanelToggle(sf::Vector2f mouse) {
            float toggleY = BOARD_Y + BOARD_PIXEL_H - 36.f;
            if (mouse.x < LEFT_PANEL_W && mouse.y >= toggleY && mouse.y < toggleY + 30.f) {
                powerSelectMode = !powerSelectMode;
                gameState.freezeFalling(powerSelectMode);
            }
        }

        void addPowerBlastParticles(sf::Color c) {
            for (int i = 0; i < 40; ++i) {
                SimpleParticle p;
                p.pos = {BOARD_X + BOARD_PIXEL_W * 0.5f, BOARD_Y + BOARD_PIXEL_H * 0.5f};
                float a = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
                float s = 100.f + static_cast<float>(rand() % 250);
                p.vel = {std::cos(a) * s, std::sin(a) * s};
                p.color = c;
                p.maxLife = p.life = 0.5f + static_cast<float>(rand() % 40) / 100.f;
                p.size = 6.f + static_cast<float>(rand() % 6);
                particles.push_back(p);
            }
        }

        // YENİ EKLENEN: Gücün asıl işini yapan çekirdek fonksiyon
        void applyPowerEffect(PowerType p) {
            switch (p) {
                case PowerType::I_Small: clearBestRow(1);
                    break;
                case PowerType::I_Big: clearBestRow(2);
                    break;
                case PowerType::O_Small: gameState.applyTimeStop(4.0f);
                    break;
                case PowerType::O_Big: gameState.applyTimeStop(8.0f);
                    break;
                case PowerType::T_Big: gameState.freeReroll(3);
                    break;
                case PowerType::L_Small: gameState.applyRotateFalling(1, true);
                    break;
                case PowerType::L_Big: gameState.applyRotateFalling(2, true);
                    break;
                case PowerType::J_Small: gameState.applyRotateFalling(1, false);
                    break;
                case PowerType::J_Big: gameState.applyRotateFalling(2, false);
                    break;
                case PowerType::S_Small: gameState.applyColumnLower(1, 1);
                    break;
                case PowerType::S_Big: gameState.applyColumnLower(2, 1);
                    break;
                case PowerType::SZ_Special: gameState.applyColumnLower(1, 2);
                    break;
                case PowerType::Z_Small: gameState.applyRandomBlockClear(1);
                    break;
                case PowerType::Z_Big: gameState.applyRandomBlockClear(3);
                    break;
                default: break;
            }

            // Güç kullanıldıktan sonra o anki parçayı en üste al
            gameState.fallingPiece.row = 0;
            gameState.fallAccum = 0.f;
        }

        // GÜNCELLENEN: Sol panelden güç tıklandığında çalışan fonksiyon
        void executePower(int index, sf::Vector2f /*mousePos*/) {
            PowerType p = gameState.slotMachine.powers[index].type;
            sf::Color powerColor = gameState.slotMachine.powers[index].color();

            // Listedeki gücü "kullanıldı" olarak işaretle/sil
            gameState.activatePower(index);

            addPowerBlastParticles(powerColor);
            applyPowerEffect(p); // Asıl etkiyi tetikle
        }

        void clearBestRow(int count) {
            std::vector<std::pair<int, int> > rowFill;
            for (int r = 0; r < BOARD_ROWS; ++r) {
                int filled = 0;
                for (int c = 0; c < BOARD_COLS; ++c)
                    if (!gameState.board.isEmpty(c, r)) filled++;
                if (filled > 0) rowFill.emplace_back(filled, r);
            }
            std::sort(rowFill.rbegin(), rowFill.rend());
            int cleared = std::min(count, static_cast<int>(rowFill.size()));
            std::vector<int> targetRows;
            for (int i = 0; i < cleared; ++i)
                targetRows.push_back(rowFill[i].second);
            std::sort(targetRows.begin(), targetRows.end());
            for (int r: targetRows)
                gameState.applyClearRow(r);
        }

        void renderOPlacementHint(sf::RenderWindow &renderWindow) {
            if (!fontLoaded) return;
            sf::RectangleShape overlay(sf::Vector2f{float(BOARD_PIXEL_W), 40.f});
            overlay.setPosition(sf::Vector2f{BOARD_X, BOARD_Y - 44.f});
            overlay.setFillColor(sf::Color(60, 20, 100, 200));
            overlay.setOutlineColor(sf::Color(180, 80, 255, 220));
            overlay.setOutlineThickness(1.5f);
            renderWindow.draw(overlay);

            sf::Text hint(font, "Click the column where you want to drop the piece!", 12);
            hint.setFillColor(sf::Color(220, 180, 255));
            auto b = hint.getLocalBounds();
            hint.setPosition(sf::Vector2f{
                BOARD_X + (BOARD_PIXEL_W - b.size.x) * 0.5f,
                BOARD_Y - 40.f
            });
            renderWindow.draw(hint);
        }

        void renderSlotResult(sf::RenderWindow &renderWindow) {
            if (!lastSlotResult.hasCombo) return;
            float alpha = std::min(slotResultTimer / 0.5f, 1.f);
            if (slotResultTimer < 0.5f) alpha = slotResultTimer / 0.5f;

            sf::RectangleShape banner(sf::Vector2f{300.f, 60.f});
            banner.setPosition(sf::Vector2f{
                BOARD_X + (BOARD_PIXEL_W - 300.f) * 0.5f,
                BOARD_Y + BOARD_PIXEL_H * 0.3f
            });
            banner.setFillColor(sf::Color(20, 20, 40,
                                          static_cast<std::uint8_t>(200 * alpha)));
            banner.setOutlineColor(sf::Color(200, 200, 80,
                                             static_cast<std::uint8_t>(255 * alpha)));
            banner.setOutlineThickness(2.f);
            renderWindow.draw(banner);

            if (!fontLoaded) return;

            std::string comboText = (lastSlotResult.comboCount >= 3) ? "3x COMBO!" : "2x COMBO!";
            PowerSlot tempSlot;
            tempSlot.type = lastSlotResult.power;

            sf::Text t1(font, comboText, 20);
            t1.setFillColor(sf::Color(255, 220, 50, static_cast<std::uint8_t>(255 * alpha)));
            t1.setStyle(sf::Text::Bold);
            auto b1 = t1.getLocalBounds();
            t1.setPosition(sf::Vector2f{
                banner.getPosition().x + (300.f - b1.size.x) * 0.5f,
                banner.getPosition().y + 6.f
            });
            renderWindow.draw(t1);

            sf::Text t2(font, tempSlot.name(), 13);
            t2.setFillColor(sf::Color(200, 200, 255, static_cast<std::uint8_t>(220 * alpha)));
            auto b2 = t2.getLocalBounds();
            t2.setPosition(sf::Vector2f{
                banner.getPosition().x + (300.f - b2.size.x) * 0.5f,
                banner.getPosition().y + 34.f
            });
            renderWindow.draw(t2);
        }

        void drawTokenGlow(sf::RenderWindow &renderWindow) {
            auto cells = gameState.fallingPiece.cells();
            for (auto [r, c]: cells) {
                if (r < 0) continue;
                sf::CircleShape glow(CELL_SIZE * 0.6f);
                glow.setOrigin(sf::Vector2f{CELL_SIZE * 0.6f, CELL_SIZE * 0.6f});
                glow.setPosition(sf::Vector2f{
                    BOARD_X + c * CELL_SIZE + CELL_SIZE * 0.5f,
                    BOARD_Y + r * CELL_SIZE + CELL_SIZE * 0.5f
                });
                float pulse = 0.5f + 0.5f * std::sin(
                                  static_cast<float>(std::clock()) / 200.f);
                glow.setFillColor(sf::Color(255, 220, 50,
                                            static_cast<std::uint8_t>(60 + 80 * pulse)));
                renderWindow.draw(glow);
            }
        }

        void updateDragPreview() {
            if (!selectionBar.isDragging()) {
                boardRenderer.setDragPreview(std::nullopt, true);
                return;
            }
            const auto &drag = selectionBar.dragState();
            sf::FloatRect br = boardRenderer.boardRect();
            br.position.x -= 120.f;
            br.size.x += 240.f;

            if (!br.contains(drag.mousePos)) {
                boardRenderer.setDragPreview(std::nullopt, true);
                return;
            }
            int col = boardRenderer.screenXToCol(drag.mousePos.x);
            int row = static_cast<int>((drag.mousePos.y - BOARD_Y) / CELL_SIZE);
            auto [valid, placed] = gameState.board.findDropPosition(drag.piece, col, row);
            boardRenderer.setDragPreview(placed, valid);
        }

        void updateFlash(float dt) {
            if (flashDuration <= 0.f) return;
            flashTimer += dt;
            if (flashTimer >= flashDuration) {
                flashTimer = flashDuration = 0.f;
                flashRows.clear();
                boardRenderer.setFlashRows({}, 0.f);
            }
        }

        struct SimpleParticle {
            sf::Vector2f pos, vel;
            sf::Color color;
            float life, maxLife, size;
        };

        void addLineClearParticles(const std::vector<int> &rows) {
            for (int r: rows)
                for (int c = 0; c < BOARD_COLS; ++c)
                    for (int k = 0; k < 4; ++k) {
                        SimpleParticle p;
                        p.pos = {
                            BOARD_X + c * CELL_SIZE + CELL_SIZE * 0.5f,
                            BOARD_Y + r * CELL_SIZE + CELL_SIZE * 0.5f
                        };
                        float a = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
                        float s = 60.f + static_cast<float>(rand() % 120);
                        p.vel = {std::cos(a) * s, std::sin(a) * s};
                        p.color = sf::Color(
                            static_cast<std::uint8_t>(200 + rand() % 55),
                            static_cast<std::uint8_t>(200 + rand() % 55),
                            static_cast<std::uint8_t>(100 + rand() % 155), 255);
                        p.maxLife = p.life = 0.4f + static_cast<float>(rand() % 30) / 100.f;
                        p.size = 3.f + static_cast<float>(rand() % 4);
                        particles.push_back(p);
                    }
        }

        void addPlaceParticles(sf::Vector2f pos) {
            for (int k = 0; k < 8; ++k) {
                SimpleParticle p;
                p.pos = pos;
                float a = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
                float s = 40.f + static_cast<float>(rand() % 80);
                p.vel = {std::cos(a) * s, std::sin(a) * s};
                p.color = sf::Color(static_cast<std::uint8_t>(100 + rand() % 155), 200, 255, 220);
                p.maxLife = p.life = 0.25f + static_cast<float>(rand() % 20) / 100.f;
                p.size = 4.f;
                particles.push_back(p);
            }
        }

        void updateParticles(float dt) {
            for (auto &p: particles) {
                p.pos += p.vel * dt;
                p.vel.y += 200.f * dt;
                p.life -= dt;
            }
            particles.erase(
                std::remove_if(particles.begin(), particles.end(),
                               [](const SimpleParticle &p) { return p.life <= 0.f; }),
                particles.end());
        }

        void renderParticles(sf::RenderWindow &renderWindow) {
            sf::CircleShape circle;
            for (const auto &p: particles) {
                float t = p.life / p.maxLife;
                sf::Color c = p.color;
                c.a = static_cast<std::uint8_t>(t * 255);
                circle.setRadius(p.size * t);
                circle.setFillColor(c);
                float r = circle.getRadius();
                circle.setOrigin(sf::Vector2f{r, r});
                circle.setPosition(p.pos);
                renderWindow.draw(circle);
            }
        }

        void renderSidePanel(sf::RenderWindow &renderWindow) {
            float px = BOARD_X + BOARD_PIXEL_W + 10.f, py = BOARD_Y;
            float pw = float(WIN_W) - px - 5.f;

            sf::RectangleShape panel(sf::Vector2f{pw, BOARD_PIXEL_H});
            panel.setPosition(sf::Vector2f{px, py});
            panel.setFillColor(sf::Color(18, 18, 30));
            panel.setOutlineColor(sf::Color(60, 60, 100));
            panel.setOutlineThickness(1.5f);
            renderWindow.draw(panel);

            if (!fontLoaded) return;

            txt(renderWindow, "NEXT", {px + 8, py + 8}, 13, {160, 160, 220});
            for (int i = 0; i < 3; ++i)
                drawMiniPiece(renderWindow, gameState.nextPieces[i],
                              {px + 12, py + 28.f + i * 60}, 16);

            float iy = py + 220;
            txt(renderWindow, "SCORE", {px + 8, iy}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.score), {px + 8, iy + 16}, 16, {255, 255, 100});
            txt(renderWindow, "LEVEL", {px + 8, iy + 44}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.level), {px + 8, iy + 60}, 16, {100, 255, 180});
            txt(renderWindow, "LINES", {px + 8, iy + 88}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.linesCleared), {px + 8, iy + 104}, 16, {180, 220, 255});

            float jy = iy + 140;
            txt(renderWindow, "TOKENS", {px + 8, jy}, 12, {220, 200, 80});
            txt(renderWindow, std::to_string(gameState.slotMachine.tokens), {px + 8, jy + 16}, 18, {255, 220, 50});

            if (gameState.levelTransitionActive) {
                float ly = jy + 52.f;
                int secsLeft = static_cast<int>(std::ceil(gameState.levelTransitionTimer));
                float pulse = 0.5f + 0.5f * std::sin(gameState.levelTransitionTimer * 8.f);

                sf::RectangleShape box(sf::Vector2f{pw - 8.f, 52.f});
                box.setPosition(sf::Vector2f{px + 4.f, ly - 2.f});
                box.setFillColor(sf::Color(
                    static_cast<std::uint8_t>(40 + 60 * pulse),
                    static_cast<std::uint8_t>(20),
                    static_cast<std::uint8_t>(60 + 80 * pulse), 200));
                box.setOutlineColor(sf::Color(
                    static_cast<std::uint8_t>(180 + 75 * pulse),
                    static_cast<std::uint8_t>(100),
                    static_cast<std::uint8_t>(255), 220));
                box.setOutlineThickness(1.5f);
                renderWindow.draw(box);

                txt(renderWindow, "COOLDOWN", {px + 8, ly + 2.f}, 10, {200, 160, 255});
                txt(renderWindow, std::to_string(secsLeft) + "s",
                    {px + 8, ly + 18.f}, 18,
                    sf::Color(
                        static_cast<std::uint8_t>(200 + 55 * pulse),
                        static_cast<std::uint8_t>(180),
                        static_cast<std::uint8_t>(255)));
            }
        }

        void drawMiniPiece(sf::RenderWindow &renderWindow, const Piece &piece,
                           sf::Vector2f origin, float cellSize) {
            sf::RectangleShape cell(sf::Vector2f{cellSize - 2.f, cellSize - 2.f});
            cell.setFillColor(piece.color());
            sf::Color bright = piece.color();
            bright.r = static_cast<std::uint8_t>(std::min(255, int(bright.r) + 60));
            bright.g = static_cast<std::uint8_t>(std::min(255, int(bright.g) + 60));
            bright.b = static_cast<std::uint8_t>(std::min(255, int(bright.b) + 60));
            cell.setOutlineColor(bright);
            cell.setOutlineThickness(-1.5f);

            int minR = 4, maxR = -1, minC = 4, maxC = -1;
            for (auto [r,c]: piece.localCells()) {
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
            if (maxR < 0) return;

            for (auto [r,c]: piece.localCells()) {
                cell.setPosition(sf::Vector2f{
                    origin.x + (c - minC) * cellSize + 1.f,
                    origin.y + (r - minR) * cellSize + 1.f
                });
                renderWindow.draw(cell);
            }
        }

        void txt(sf::RenderWindow &renderWindow, const std::string &s,
                 sf::Vector2f pos, unsigned sz, sf::Color col) {
            sf::Text t(font, s, sz);
            t.setFillColor(col);
            t.setPosition(pos);
            renderWindow.draw(t);
        }

        void renderHUD(sf::RenderWindow & /*renderWindow*/) {
        }

        void renderOverlay(sf::RenderWindow &renderWindow,
                           const std::string &title, const std::string &sub) {
            sf::RectangleShape dim(sf::Vector2f{float(WIN_W), float(WIN_H)});
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            renderWindow.draw(dim);
            if (!fontLoaded) return;

            sf::Text t(font, title, 38);
            t.setFillColor(sf::Color(255, 220, 60));
            t.setStyle(sf::Text::Bold);
            auto b = t.getLocalBounds();
            t.setPosition(sf::Vector2f{(WIN_W - b.size.x) * 0.5f, WIN_H * 0.35f});
            renderWindow.draw(t);

            sf::Text s(font, sub, 18);
            s.setFillColor(sf::Color(200, 200, 255));
            auto sb = s.getLocalBounds();
            s.setPosition(sf::Vector2f{(WIN_W - sb.size.x) * 0.5f, WIN_H * 0.35f + 50.f});
            renderWindow.draw(s);
        }

        void renderPowerModal(sf::RenderWindow &renderWindow) {
            // 1. Arka planı tamamen karart
            sf::RectangleShape dim(sf::Vector2f{float(WIN_W), float(WIN_H)});
            dim.setFillColor(sf::Color(0, 0, 0, 190));
            renderWindow.draw(dim);

            if (!fontLoaded) return;

            // 2. Ana Modal Kutusu
            float mw = 360.f, mh = 210.f;
            float mx = (WIN_W - mw) * 0.5f;
            float my = (WIN_H - mh) * 0.5f;

            sf::RectangleShape modal(sf::Vector2f{mw, mh});
            modal.setPosition(sf::Vector2f{mx, my});
            modal.setFillColor(sf::Color(25, 25, 40));
            modal.setOutlineColor(pendingPower.color());
            modal.setOutlineThickness(3.f);
            renderWindow.draw(modal);

            // 3. Üst Başlık
            sf::Text title(font, "NEW POWER EARNED!", 22);
            title.setFillColor(sf::Color(255, 220, 50));
            title.setStyle(sf::Text::Bold);
            auto b1 = title.getLocalBounds();
            title.setPosition(sf::Vector2f{mx + (mw - b1.size.x) * 0.5f, my + 25.f});
            renderWindow.draw(title);

            // 4. Gücün Adı ve Açıklaması
            sf::Text pName(font, pendingPower.name(), 18);
            pName.setFillColor(pendingPower.color());
            auto b2 = pName.getLocalBounds();
            pName.setPosition(sf::Vector2f{mx + (mw - b2.size.x) * 0.5f, my + 80.f});
            renderWindow.draw(pName);

            // Fare pozisyonunu butonların hover efekti için alıyoruz
            sf::Vector2f mouse = sf::Vector2f(sf::Mouse::getPosition(window));

            // 5. ŞİMDİ KULLAN Butonu
            // SFML 3 stili: sf::FloatRect({pozisyonX, pozisyonY}, {genişlik, yükseklik})
            btnUseNowRect = sf::FloatRect({mx + 30.f, my + 140.f}, {140.f, 45.f});
            bool hoverUse = btnUseNowRect.contains(mouse);

            // btnUseNowRect.size direkt sf::Vector2f döndürür
            sf::RectangleShape btnUse(btnUseNowRect.size);
            btnUse.setPosition(btnUseNowRect.position);
            btnUse.setFillColor(hoverUse ? sf::Color(60, 180, 60) : sf::Color(40, 120, 40));
            btnUse.setOutlineColor(sf::Color(100, 255, 100));
            btnUse.setOutlineThickness(2.f);
            renderWindow.draw(btnUse);

            sf::Text txtUse(font, "USE NOW", 14);
            txtUse.setFillColor(sf::Color::White);
            txtUse.setStyle(sf::Text::Bold);
            auto b3 = txtUse.getLocalBounds();
            txtUse.setPosition(sf::Vector2f{
                btnUseNowRect.position.x + (btnUseNowRect.size.x - b3.size.x) * 0.5f,
                btnUseNowRect.position.y + (btnUseNowRect.size.y - b3.size.y) * 0.5f - 4.f
            });
            renderWindow.draw(txtUse);

            // 6. SAKLA Butonu
            btnKeepRect = sf::FloatRect({mx + 190.f, my + 140.f}, {140.f, 45.f});
            bool hoverKeep = btnKeepRect.contains(mouse);

            sf::RectangleShape btnKeep(btnKeepRect.size);
            btnKeep.setPosition(btnKeepRect.position);
            btnKeep.setFillColor(hoverKeep ? sf::Color(100, 100, 140) : sf::Color(60, 60, 90));
            btnKeep.setOutlineColor(sf::Color(150, 150, 255));
            btnKeep.setOutlineThickness(2.f);
            renderWindow.draw(btnKeep);

            sf::Text txtKeep(font, "KEEP", 14);
            txtKeep.setFillColor(sf::Color::White);
            txtKeep.setStyle(sf::Text::Bold);
            auto b4 = txtKeep.getLocalBounds();
            txtKeep.setPosition(sf::Vector2f{
                btnKeepRect.position.x + (btnKeepRect.size.x - b4.size.x) * 0.5f,
                btnKeepRect.position.y + (btnKeepRect.size.y - b4.size.y) * 0.5f - 4.f
            });
            renderWindow.draw(txtKeep);
        }

        void renderDebugMenu(sf::RenderWindow &renderWindow) {
            if (!fontLoaded) return;

            // Arka planı hafifçe karart
            sf::RectangleShape dim(sf::Vector2f{float(WIN_W), float(WIN_H)});
            dim.setFillColor(sf::Color(0, 0, 0, 200));
            renderWindow.draw(dim);

            // Başlık
            sf::Text title(font, ">> DEVELOPER TEST MENU << (Press F1 to close)", 18);
            title.setFillColor(sf::Color(100, 255, 150));
            title.setStyle(sf::Text::Bold);
            title.setPosition({60.f, 40.f});
            renderWindow.draw(title);

            // Grid Ayarları
            float startX = 60.f, startY = 80.f;
            float btnW = 180.f, btnH = 40.f;
            float spacingX = 15.f, spacingY = 15.f;
            sf::Vector2f mouse = sf::Vector2f(sf::Mouse::getPosition(window));

            for (int i = 0; i < 15; ++i) {
                int col = i % 3;
                int row = i / 3;

                sf::FloatRect btnRect({startX + col * (btnW + spacingX), startY + row * (btnH + spacingY)}, {btnW, btnH});
                bool hover = btnRect.contains(mouse);

                PowerSlot dummySlot;
                dummySlot.type = static_cast<PowerType>(i);
                sf::Color pColor = dummySlot.color();

                sf::RectangleShape btn(btnRect.size);
                btn.setPosition(btnRect.position);
                btn.setFillColor(hover ? sf::Color(pColor.r / 3, pColor.g / 3, pColor.b / 3, 255) : sf::Color(30, 30, 40, 255));
                btn.setOutlineColor(pColor);
                btn.setOutlineThickness(hover ? 2.f : 1.f);
                renderWindow.draw(btn);

                sf::Text txt(font, dummySlot.name(), 12);
                txt.setFillColor(sf::Color::White);
                auto b = txt.getLocalBounds();
                txt.setPosition({
                    btnRect.position.x + (btnRect.size.x - b.size.x) * 0.5f,
                    btnRect.position.y + (btnRect.size.y - b.size.y) * 0.5f - 3.f
                });
                renderWindow.draw(txt);
            }
        }

        void restartGame() {
            gameState = GameState(static_cast<std::uint32_t>(std::time(nullptr)));
            lastTokenMilestone = 0;
            setupCallbacks();
            audio.resumeMusic();
            showGameOver = false;
            particles.clear();
            flashRows.clear();
            flashTimer = flashDuration = 0.f;
            powerSelectMode = false;
        }

        // ── Member değişkenler ────────────────────────────────────────────────────
        sf::RenderWindow &window;
        GameState gameState;
        BoardRenderer boardRenderer;
        SelectionBar selectionBar;
        engine::AudioManager audio;
        sf::Font font;
        bool fontLoaded{false};
        bool showGameOver{false};

        // Flash
        std::vector<int> flashRows;
        float flashTimer{0.f}, flashDuration{0.f};

        // Parçacıklar
        std::vector<SimpleParticle> particles;

        // Slot machine sonuç gösterimi
        SlotResult lastSlotResult;
        float slotResultTimer{0.f};

        // Güç seçim modu
        bool powerSelectMode{false};
        int selectedPowerIdx{-1};
        int hoveredPowerIdx{-1};

        // O_Big: düşen parçayı dondurdu, oyuncu board'a tıklayacak
        bool oPlacementMode{false};

        // Sonraki parça ok animasyonu için zaman fazı
        float arrowPhase{0.f};

        // 1000 puan jeton bildirimi
        float tokenMilestoneNotifTimer{0.f};
        int lastTokenMilestone{0};

        // Modal State
        bool showPowerModal{false};
        PowerSlot pendingPower; // Gösterilecek güç
        sf::FloatRect btnUseNowRect; // Şimdi kullan butonu çarpışma alanı
        sf::FloatRect btnKeepRect; // Sakla butonu çarpışma alanı

        // Spin Animasyonu Bekleme
        float spinWaitTimer{0.f};
        bool isSpinning{false};

        // Debug Menü Bayrağı
        bool showDebugMenu{false};
    };
} // namespace tetris