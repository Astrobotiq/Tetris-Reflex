#pragma once

#include "../SceneManager.hpp"
#include "GameState.hpp"
#include "BoardRenderer.hpp"
#include "SelectionBar.hpp"
#include "ParticleEffect.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <ctime>
#include <memory>
#include <string>

namespace tetris {
    inline constexpr unsigned WIN_W = 700; // Genişletildi: sol panel için yer
    inline constexpr unsigned WIN_H = 740;

    inline constexpr float LEFT_PANEL_W = 140.f; // Sol panel genişliği
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
            gameState.onLinesCleared = [this](int, std::vector<int> rows) {
                flashRows = rows;
                flashTimer = 0.f;
                flashDuration = 0.35f;
                addLineClearParticles(rows);
                // 1000 puan jeton milestone bildirimi
                static int lastMilestone = 0;
                if (gameState.scoreTokenMilestone > lastMilestone) {
                    lastMilestone = gameState.scoreTokenMilestone;
                    tokenMilestoneNotifTimer = 2.0f;
                }
            };
            gameState.onGameOver = [this]() { showGameOver = true; };

            // Slot machine sonucu — combo varsa efekt göster
            gameState.onSlotResult = [this](SlotResult result) {
                lastSlotResult = result;
                slotResultTimer = 2.5f; // 2.5 saniye göster
            };
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
                if (font.openFromFile(path)) {
                    loaded = true;
                }
            }
            if (loaded) {
                selectionBar.setFont(font);
                fontLoaded = true;
            }
            particles.reserve(512);

            // Güç seçim modu başlangıçta kapalı
            powerSelectMode = false;
            selectedPowerIdx = -1;
        }

        void update(float dt, engine::InputManager &input) override {
            if (gameState.gameOver) {
                if (input.isPressed(sf::Keyboard::Key::R)) restartGame();
                return;
            }
            if (input.isPressed(sf::Keyboard::Key::P))
                gameState.paused = !gameState.paused;

            gameState.update(dt);
            selectionBar.update(dt, gameState);

            updateFlash(dt);
            updateParticles(dt);
            updateDragPreview();

            // Ok animasyonu fazı (sürekli döngü, 1.2 sn periyot)
            arrowPhase += dt / 1.2f;
            if (arrowPhase > 1.f) arrowPhase -= 1.f;

            // Jeton milestone bildirimi sayacı
            if (tokenMilestoneNotifTimer > 0.f) tokenMilestoneNotifTimer -= dt;

            // Slot result ekranda ne kadar kalsın
            if (slotResultTimer > 0.f) slotResultTimer -= dt;
        }

        void handleEvent(const sf::Event &event) {
            if (gameState.gameOver) return;
            sf::Vector2f mouse = sf::Vector2f(sf::Mouse::getPosition(window));

            if (const auto *e = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (e->button == sf::Mouse::Button::Left) {
                    // O_Big modu: board üzerinde tıklanan kolona düşen parçayı taşı
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

                    // Fare sol panelin üzerindeyse tıklamaları panele yönlendir
                    if (mouse.x < LEFT_PANEL_W && mouse.y < BAR_Y) {
                        if (powerSelectMode) {
                            handlePowerClick(mouse);
                        } else {
                            handleLeftPanelToggle(mouse);
                        }
                        return;
                    }

                    // Güç seçme modundayken sağ taraftaki board'a veya boşa tıklanırsa modu iptal et
                    if (powerSelectMode) {
                        powerSelectMode = false;
                        gameState.freezeFalling(false); // DÜZELTME: Moddan çıkınca zamanı tekrar akıt
                        return;
                    }

                    // Sol panel haricindeki tıklamalar seçim barına gider
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

            // Bir sonraki parçanın düşeceği sütuna akan ok göstergesi
            if (!gameState.gameOver && !gameState.paused)
                renderNextPieceArrow(renderWindow);

            // Jeton taşıyan düşen parça üzerinde parlama
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
                    sf::Text pt(font, "GUC SECIMI...", 20);
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

            // O_Big modu: board üzerinde ipucu göster
            if (oPlacementMode)
                renderOPlacementHint(renderWindow);

            // Slot machine sonuç bildirimi
            if (slotResultTimer > 0.f)
                renderSlotResult(renderWindow);

            // 1000 puan jeton bildirimi
            if (tokenMilestoneNotifTimer > 0.f)
                renderTokenMilestoneNotif(renderWindow);

            renderWindow.setView(renderWindow.getDefaultView());
            renderHUD(renderWindow);

            if (gameState.paused) renderOverlay(renderWindow, "DURDURULDU", "P: devam");
            if (showGameOver)
                renderOverlay(renderWindow, "OYUN BITTI",
                              "Skor: " + std::to_string(gameState.score) + "   R: Yeniden");
        }

    private:
        // ── Sonraki parçanın düşeceği sütun — akan ok göstergesi ─────────────────
        void renderNextPieceArrow(sf::RenderWindow &renderWindow) {
            // nextPieces[0]'ın genişliğini hesapla
            const Piece &nextP = gameState.nextPieces[0];
            int minC = 4, maxC = -1;
            for (auto [r, c]: nextP.localCells()) {
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
            if (maxC < 0) return;
            int pieceWidth = maxC - minC + 1;

            int startCol = gameState.nextPieceCol + minC;
            int endCol = gameState.nextPieceCol + maxC;

            // Sütun aralığını tahta sınırlarına kısıt
            startCol = std::max(0, std::min(startCol, BOARD_COLS - 1));
            endCol = std::max(0, std::min(endCol, BOARD_COLS - 1));

            // Parçanın rengini soluk kullan
            sf::Color arrowColor = nextP.color();
            arrowColor.a = 0; // alpha aşağıda per-ok hesaplanacak

            // 3 adet ok üst üste kayar (arrowPhase ile ofset)
            constexpr int ARROW_COUNT = 3;
            constexpr float ARROW_SPACING = 1.f / ARROW_COUNT; // fazlarda
            constexpr int ARROW_H = 10; // piksel yüksekliği
            constexpr int ARROW_W = CELL_SIZE - 6; // genişlik

            // Board'un üstünden ok başlangıç alanı (BOARD_Y - 28 piksel yukarısı)
            float topY = BOARD_Y - 32.f;

            sf::ConvexShape arrowShape(3); // üçgen ok

            for (int col = startCol; col <= endCol; ++col) {
                float cx = BOARD_X + col * CELL_SIZE + CELL_SIZE * 0.5f;

                for (int k = 0; k < ARROW_COUNT; ++k) {
                    // Her ok kendi fazından hareketle aşağı doğru kayar (0→1 arası)
                    float phase = std::fmod(arrowPhase + k * ARROW_SPACING, 1.f);

                    // 0 = üst (topY), 1 = tahtanın üstü (BOARD_Y - 4)
                    float travel = (BOARD_Y - 4.f) - topY;
                    float y = topY + phase * travel;

                    // Fazın ortasında tam görünür, uçlarda soluk (fade in/out)
                    float fade = std::sin(phase * 3.14159f); // 0→1→0
                    std::uint8_t alpha = static_cast<std::uint8_t>(fade * 200.f);

                    sf::Color c = nextP.color();
                    c.a = alpha;

                    // Üçgen ok (aşağı bakan)
                    arrowShape.setPoint(0, {cx - ARROW_W * 0.5f, y});
                    arrowShape.setPoint(1, {cx + ARROW_W * 0.5f, y});
                    arrowShape.setPoint(2, {cx, y + ARROW_H});
                    arrowShape.setFillColor(c);
                    renderWindow.draw(arrowShape);
                }
            }

            // Hedef sütunların üzerinde ince dikey şerit (tahtanın tamamı boyunca)
            for (int col = startCol; col <= endCol; ++col) {
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
            float t = tokenMilestoneNotifTimer / 2.0f; // 0→1
            float alpha = t < 0.2f ? (t / 0.2f) : (t > 0.8f ? ((1.f - t) / 0.2f) : 1.f);
            std::uint8_t a = static_cast<std::uint8_t>(alpha * 230);

            // Arka plan kutusu
            float bw = 210.f, bh = 44.f;
            float bx = BOARD_X + (BOARD_PIXEL_W - bw) * 0.5f;
            float by = BOARD_Y + 12.f;
            sf::RectangleShape box({bw, bh});
            box.setPosition({bx, by});
            box.setFillColor(sf::Color(30, 20, 50, a));
            box.setOutlineColor(sf::Color(220, 180, 60, a));
            box.setOutlineThickness(2.f);
            renderWindow.draw(box);

            sf::Text t1(font, "+1 JETON", 15);
            t1.setFillColor(sf::Color(255, 220, 50, a));
            t1.setStyle(sf::Text::Bold);
            auto b1 = t1.getLocalBounds();
            t1.setPosition({bx + (bw - b1.size.x) * 0.5f, by + 4.f});
            renderWindow.draw(t1);

            sf::Text t2(font, "1000 puan esigi!", 11);
            t2.setFillColor(sf::Color(200, 170, 255, a));
            auto b2 = t2.getLocalBounds();
            t2.setPosition({bx + (bw - b2.size.x) * 0.5f, by + 24.f});
            renderWindow.draw(t2);
        }

        // ── Sol panel: güçler listesi ─────────────────────────────────────────────
        void renderLeftPanel(sf::RenderWindow &renderWindow) {
            // Arka plan
            sf::RectangleShape panel(sf::Vector2f{LEFT_PANEL_W, BOARD_PIXEL_H});
            panel.setPosition(sf::Vector2f{0.f, BOARD_Y});
            panel.setFillColor(sf::Color(18, 18, 32));
            panel.setOutlineColor(sf::Color(60, 60, 100));
            panel.setOutlineThickness(1.5f);
            renderWindow.draw(panel);

            if (!fontLoaded) return;

            // Başlık
            txt(renderWindow, "GUCLER", {6.f, BOARD_Y + 8.f}, 13, sf::Color(180, 180, 255));

            auto &powers = gameState.slotMachine.powers;

            if (powers.empty()) {
                txt(renderWindow, "(bos)", {10.f, BOARD_Y + 30.f}, 11, sf::Color(80, 80, 100));
            }

            for (int i = 0; i < static_cast<int>(powers.size()); ++i) {
                float y = BOARD_Y + 28.f + i * 44.f;

                // Buton arka planı
                sf::RectangleShape btn(sf::Vector2f{LEFT_PANEL_W - 8.f, 38.f});
                btn.setPosition(sf::Vector2f{4.f, y});

                bool hovered = powerSelectMode && hoveredPowerIdx == i;
                sf::Color c = powers[i].color();
                btn.setFillColor(sf::Color(c.r / 4, c.g / 4, c.b / 4, hovered ? 220 : 160));
                btn.setOutlineColor(hovered ? c : sf::Color(c.r / 2, c.g / 2, c.b / 2, 160));
                btn.setOutlineThickness(hovered ? 2.f : 1.f);
                renderWindow.draw(btn);

                // Güç adı (2 satıra böl)
                std::string name = powers[i].name();
                txt(renderWindow, name, {8.f, y + 4.f}, 10, sf::Color(c.r, c.g, c.b, 220));

                // "KULLAN" label
                if (powerSelectMode)
                    txt(renderWindow, "tikla!", {8.f, y + 20.f}, 9, sf::Color(200, 200, 100));
            }

            // "Güç kullan" modu toggle butonu
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
                txt(renderWindow, powerSelectMode ? "IPTAL" : "GUC KULLAN",
                    {8.f, btnY + 7.f}, 11, sf::Color::White);
            }
        }

        // Güç butonuna tıklama
        void handlePowerClick(sf::Vector2f mouse) {
            auto &powers = gameState.slotMachine.powers;

            float toggleY = BOARD_Y + BOARD_PIXEL_H - 36.f;
            if (mouse.x < LEFT_PANEL_W && mouse.y >= toggleY && mouse.y < toggleY + 30.f) {
                powerSelectMode = !powerSelectMode;
                gameState.freezeFalling(powerSelectMode); // DÜZELTME
                return;
            }

            for (int i = 0; i < static_cast<int>(powers.size()); ++i) {
                float y = BOARD_Y + 28.f + i * 44.f;
                if (mouse.x >= 4.f && mouse.x < LEFT_PANEL_W - 4.f &&
                    mouse.y >= y && mouse.y < y + 38.f) {
                    executePower(i, mouse);
                    powerSelectMode = false;

                    // Eğer kullandığı güç O_Big (Tahtaya tıklayıp yerleştirme) değilse oyunu akıt
                    if (!oPlacementMode) {
                        gameState.freezeFalling(false);
                    }
                    return;
                }
            }
        }

        // "Güç kullan" toggle butonuna tıklama (sol panelin dışından da erişilebilir)
        void handleLeftPanelToggle(sf::Vector2f mouse) {
            float toggleY = BOARD_Y + BOARD_PIXEL_H - 36.f;
            if (mouse.x < LEFT_PANEL_W && mouse.y >= toggleY && mouse.y < toggleY + 30.f) {
                powerSelectMode = !powerSelectMode;
                gameState.freezeFalling(powerSelectMode); // DÜZELTME: Seçim anında oyunu dondur
            }
        }

        // YENİ GÖRSEL EFEKT: Güç kullanıldığında tahtanın ortasından patlayan renkli ışıklar
        void addPowerBlastParticles(sf::Color c) {
            for (int i = 0; i < 40; ++i) {
                SimpleParticle p;
                p.pos = {BOARD_X + BOARD_PIXEL_W * 0.5f, BOARD_Y + BOARD_PIXEL_H * 0.5f};
                float a = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
                float s = 100.f + static_cast<float>(rand() % 250); // Hızlı ve büyük patlama
                p.vel = {std::cos(a) * s, std::sin(a) * s};
                p.color = c;
                p.maxLife = p.life = 0.5f + static_cast<float>(rand() % 40) / 100.f;
                p.size = 6.f + static_cast<float>(rand() % 6);
                particles.push_back(p);
            }
        }

        // ── Güç uygula ────────────────────────────────────────────────────────────
        void executePower(int index, sf::Vector2f /*mousePos*/) {
            // Gücü silmeden önce rengini alıp görsel patlamayı tetikle!
            sf::Color powerColor = gameState.slotMachine.powers[index].color();
            addPowerBlastParticles(powerColor);

            PowerType p = gameState.activatePower(index);
            switch (p) {
                // I: satır temizle — en dolu satırı otomatik seç
                case PowerType::I_Small: clearBestRow(1);
                    break;
                case PowerType::I_Big: clearBestRow(2);
                    break;

                // O: Zamanı durdur
                case PowerType::O_Small:
                    gameState.applyTimeStop(4.0f);
                    break;
                case PowerType::O_Big:
                    gameState.applyTimeStop(8.0f);
                    break;

                // T: selection parçaları yenile (jetonsuz)
                case PowerType::T_Small:
                    gameState.freeReroll(1);
                    break;
                case PowerType::T_Big:
                    gameState.freeReroll(3);
                    break;

                // L / J: döndür
                case PowerType::L_Small: gameState.applyRotateFalling(1, true);
                    break;
                case PowerType::L_Big: gameState.applyRotateFalling(2, true);
                    break;
                case PowerType::J_Small: gameState.applyRotateFalling(1, false);
                    break;
                case PowerType::J_Big: gameState.applyRotateFalling(2, false);
                    break;

                // S: en yüksek sütun(lar)ı indir
                case PowerType::S_Small: gameState.applyColumnLower(1, 1);
                    break;
                case PowerType::S_Big: gameState.applyColumnLower(2, 1);
                    break;
                case PowerType::SZ_Special: gameState.applyColumnLower(1, 2);
                    break;

                // Z: rastgele blok patlat
                case PowerType::Z_Small: gameState.applyRandomBlockClear(1);
                    break;
                case PowerType::Z_Big: gameState.applyRandomBlockClear(3);
                    break;

                default: break;
            }
        }

        // En dolu N satırı temizle (I gücü için)
        void clearBestRow(int count) {
            // En fazla dolu hücreye sahip satırları bul
            std::vector<std::pair<int, int> > rowFill; // (dolu hücre sayısı, satır index)
            for (int r = 0; r < BOARD_ROWS; ++r) {
                int filled = 0;
                for (int c = 0; c < BOARD_COLS; ++c)
                    if (!gameState.board.isEmpty(c, r)) filled++;
                if (filled > 0) rowFill.emplace_back(filled, r);
            }

            std::sort(rowFill.rbegin(), rowFill.rend());
            int cleared = std::min(count, static_cast<int>(rowFill.size()));

            std::vector<int> targetRows;
            for (int i = 0; i < cleared; ++i) {
                targetRows.push_back(rowFill[i].second);
            }
            std::sort(targetRows.begin(), targetRows.end());

            for (int r: targetRows) {
                gameState.applyClearRow(r);
            }
        }

        // ── O_Big: kolon seçim ipucu ─────────────────────────────────────────────
        void renderOPlacementHint(sf::RenderWindow &renderWindow) {
            if (!fontLoaded) return;

            // Yarı şeffaf overlay
            sf::RectangleShape overlay(sf::Vector2f{float(BOARD_PIXEL_W), 40.f});
            overlay.setPosition(sf::Vector2f{BOARD_X, BOARD_Y - 44.f});
            overlay.setFillColor(sf::Color(60, 20, 100, 200));
            overlay.setOutlineColor(sf::Color(180, 80, 255, 220));
            overlay.setOutlineThickness(1.5f);
            renderWindow.draw(overlay);

            sf::Text hint(font, "Dusen parcayi koymak istedigin kolona tikla!", 12);
            hint.setFillColor(sf::Color(220, 180, 255));
            auto b = hint.getLocalBounds();
            hint.setPosition(sf::Vector2f{
                BOARD_X + (BOARD_PIXEL_W - b.size.x) * 0.5f,
                BOARD_Y - 40.f
            });
            renderWindow.draw(hint);
        }

        // ── Slot sonuç banner ─────────────────────────────────────────────────────
        void renderSlotResult(sf::RenderWindow &renderWindow) {
            if (!lastSlotResult.hasCombo) return;

            float alpha = std::min(slotResultTimer / 0.5f, 1.f); // Fade in/out
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

            std::string comboText = (lastSlotResult.comboCount >= 3)
                                        ? "3x KOMBO!"
                                        : "2x KOMBO!";
            PowerSlot tempSlot;
            tempSlot.type = lastSlotResult.power;
            std::string powerText = tempSlot.name();

            sf::Text t1(font, comboText, 20);
            t1.setFillColor(sf::Color(255, 220, 50,
                                      static_cast<std::uint8_t>(255 * alpha)));
            t1.setStyle(sf::Text::Bold);
            auto b1 = t1.getLocalBounds();
            t1.setPosition(sf::Vector2f{
                banner.getPosition().x + (300.f - b1.size.x) * 0.5f,
                banner.getPosition().y + 6.f
            });
            renderWindow.draw(t1);

            sf::Text t2(font, powerText, 13);
            t2.setFillColor(sf::Color(200, 200, 255,
                                      static_cast<std::uint8_t>(220 * alpha)));
            auto b2 = t2.getLocalBounds();
            t2.setPosition(sf::Vector2f{
                banner.getPosition().x + (300.f - b2.size.x) * 0.5f,
                banner.getPosition().y + 34.f
            });
            renderWindow.draw(t2);
        }

        // ── Jeton parıltısı ───────────────────────────────────────────────────────
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

        // ── Drag preview ─────────────────────────────────────────────────────────
        void updateDragPreview() {
            if (!selectionBar.isDragging()) {
                boardRenderer.setDragPreview(std::nullopt, true);
                return;
            }
            const auto &drag = selectionBar.dragState();
            sf::FloatRect br = boardRenderer.boardRect();

            // DÜZELTME: SFML 3 formatına göre esneklik payı
            br.position.x -= 120.f; // Sola doğru 120 piksel esneklik
            br.size.x += 240.f; // Toplam genişliği artırarak sağa da esneklik

            if (!br.contains(drag.mousePos)) {
                boardRenderer.setDragPreview(std::nullopt, true);
                return;
            }
            int col = boardRenderer.screenXToCol(drag.mousePos.x);
            int row = static_cast<int>((drag.mousePos.y - BOARD_Y) / CELL_SIZE);
            auto [valid, placed] = gameState.board.findDropPosition(drag.piece, col, row);
            boardRenderer.setDragPreview(placed, valid);
        }

        // ── Flash ─────────────────────────────────────────────────────────────────
        void updateFlash(float dt) {
            if (flashDuration <= 0.f) return;
            flashTimer += dt;
            if (flashTimer >= flashDuration) {
                flashTimer = flashDuration = 0.f;
                flashRows.clear();
                boardRenderer.setFlashRows({}, 0.f);
            }
        }

        // ── Parçacıklar ───────────────────────────────────────────────────────────
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
                            BOARD_X + c * CELL_SIZE + CELL_SIZE * 0.5f, BOARD_Y + r * CELL_SIZE + CELL_SIZE * 0.5f
                        };
                        float a = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
                        float s = 60.f + static_cast<float>(rand() % 120);
                        p.vel = {std::cos(a) * s, std::sin(a) * s};
                        p.color = sf::Color(static_cast<std::uint8_t>(200 + rand() % 55),
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

        // ── Sağ panel ─────────────────────────────────────────────────────────────
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

            txt(renderWindow, "SONRAKI", {px + 8, py + 8}, 13, {160, 160, 220});
            for (int i = 0; i < 3; ++i)
                drawMiniPiece(renderWindow, gameState.nextPieces[i],
                              {px + 12, py + 28.f + i * 60}, 16);

            float iy = py + 220;
            txt(renderWindow, "SKOR", {px + 8, iy}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.score), {px + 8, iy + 16}, 16, {255, 255, 100});
            txt(renderWindow, "SEVIYE", {px + 8, iy + 44}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.level), {px + 8, iy + 60}, 16, {100, 255, 180});
            txt(renderWindow, "SATIRLAR", {px + 8, iy + 88}, 12, {140, 140, 200});
            txt(renderWindow, std::to_string(gameState.linesCleared), {px + 8, iy + 104}, 16, {180, 220, 255});

            // Jeton
            float jy = iy + 140;
            txt(renderWindow, "JETON", {px + 8, jy}, 12, {220, 200, 80});
            txt(renderWindow, std::to_string(gameState.slotMachine.tokens), {px + 8, jy + 16}, 18, {255, 220, 50});

            // Level geçiş dondurması geri sayımı
            if (gameState.levelTransitionActive) {
                float ly = jy + 52.f;
                int secsLeft = static_cast<int>(std::ceil(gameState.levelTransitionTimer));

                // Yanıp sönen arka plan kutusu
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

                txt(renderWindow, "BEKLEME", {px + 8, ly + 2.f}, 10, {200, 160, 255});
                txt(renderWindow, std::to_string(secsLeft) + "sn",
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
                    origin.x + (c - minC) * cellSize + 1.f, origin.y + (r - minR) * cellSize + 1.f
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
            // HUD alanı reserved — ileride can barı vb.
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

        void restartGame() {
            gameState = GameState(static_cast<std::uint32_t>(std::time(nullptr)));
            gameState.onLinesCleared = [this](int, std::vector<int> rows) {
                flashRows = rows;
                flashTimer = 0.f;
                flashDuration = 0.35f;
                addLineClearParticles(rows);
                // 1000 puan jeton milestone bildirimi
                static int lastMilestone = 0;
                lastMilestone = 0; // Restart'ta sıfırla
                if (gameState.scoreTokenMilestone > lastMilestone) {
                    lastMilestone = gameState.scoreTokenMilestone;
                    tokenMilestoneNotifTimer = 2.0f;
                }
            };
            gameState.onGameOver = [this]() { showGameOver = true; };
            gameState.onSlotResult = [this](SlotResult r) {
                lastSlotResult = r;
                slotResultTimer = 2.5f;
            };
            showGameOver = false;
            particles.clear();
            flashRows.clear();
            flashTimer = flashDuration = 0.f;
            powerSelectMode = false;
        }

        sf::RenderWindow &window;
        GameState gameState;
        BoardRenderer boardRenderer;
        SelectionBar selectionBar;
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
    };
} // namespace tetris
