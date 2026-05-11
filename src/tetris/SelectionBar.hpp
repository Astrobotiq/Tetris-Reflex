#pragma once

#include "Tetromino.hpp"
#include "GameState.hpp"
#include <SFML/Graphics.hpp>
#include <optional>
#include <cmath>

namespace tetris {

inline constexpr float BAR_HEIGHT        = 130.f;
inline constexpr float PIECE_PREVIEW_CELL = 24.f;

// ─── Sürükleme durumu ────────────────────────────────────────────────────────
struct DragState {
    int          selectionIndex { -1 };
    Piece        piece;
    sf::Vector2f mousePos;
    bool         active { false };
};

// ─── Slot animasyon durumu ───────────────────────────────────────────────────
struct SlotAnimState {
    bool  spinning    { false };
    float timer       { 0.f  };
    float duration    { 2.f };  // Toplam spin süresi
    // Animasyon sırasında ekranda dönen "sahte" tipler
    std::array<TetrominoType, 3> displayTypes {
        TetrominoType::I, TetrominoType::O, TetrominoType::T
    };
};

class SelectionBar {
public:
    SelectionBar(sf::Vector2f pos, float w)
        : originPos(pos), width(w)
    {
        background.setSize(sf::Vector2f{width, BAR_HEIGHT});
        background.setPosition(originPos);
        background.setFillColor(sf::Color(20, 20, 35));
        background.setOutlineColor(sf::Color(80, 80, 130));
        background.setOutlineThickness(2.f);
    }

    void setFont(const sf::Font& f) { fontPtr = &f; }

    // ── Update (animasyon için her karede çağrıl) ─────────────────────────────
    void update(float dt, GameState& gs) {
        if (!anim.spinning) return;

        anim.timer += dt;

        // Animasyon süresince rastgele tipler göster
        float phase = anim.timer / anim.duration;
        if (phase < 1.f) {
            // Her 0.1 saniyede bir yeni rastgele tip göster
            static float flipTimer = 0.f;
            flipTimer += dt;
            if (flipTimer > 0.08f) {
                flipTimer = 0.f;
                for (int i = 0; i < 3; ++i) {
                    int t = static_cast<int>(TetrominoType::COUNT);
                    anim.displayTypes[i] = static_cast<TetrominoType>(
                        rand() % (t - 1)); // COUNT hariç
                }
            }
        } else {
            // Animasyon bitti — gerçek parçaları göster
            anim.spinning = false;
            anim.timer    = 0.f;

            // DÜZELTME: Animasyon bittiğinde oyunu tekrar akmaya bırak
            gs.freezeFalling(false);
        }
    }

    // ── Input ────────────────────────────────────────────────────────────────
    bool onMousePress(sf::Vector2f mousePos, GameState& gs) {
        // Reroll butonu tıklandı mı?
        if (rerollButtonBounds().contains(mousePos)) {
            if (gs.slotMachine.canReroll()) {
                startSpin(gs);
                return true;
            }
            return false;
        }

        // Parça slotu tıklandı mı?
        int idx = hitTest(mousePos);
        if (idx < 0 || anim.spinning) return false;

        drag.active         = true;
        drag.selectionIndex = idx;
        drag.piece          = gs.selectionPieces[idx];
        drag.mousePos       = mousePos;
        return true;
    }

    bool onRightClick(sf::Vector2f mousePos, GameState& gs) {
        int idx = hitTest(mousePos);
        if (idx < 0) return false;
        gs.rotateSelectionPiece(idx);
        if (drag.active && drag.selectionIndex == idx)
            drag.piece = gs.selectionPieces[idx];
        return true;
    }

    void onMouseMove(sf::Vector2f mousePos) {
        if (drag.active) drag.mousePos = mousePos;
    }

    struct DropResult { bool success; int slotIndex; int boardCol; int boardRow; };
    DropResult onMouseRelease(sf::Vector2f mousePos,
                              float boardOriginX, float boardOriginY,
                              float boardBottom, GameState& gs)
    {
        if (!drag.active) return {false,-1,-1,-1};
        drag.active = false;

        // DÜZELTME: X ekseninde boardOriginX'in 120 piksel soluna kadar izin ver
        bool onBoard = mousePos.y >= boardOriginY && mousePos.y <= boardBottom
                    && mousePos.x >= (boardOriginX - 120.f);

        if (!onBoard) return {false, drag.selectionIndex, -1, -1};

        int col = static_cast<int>((mousePos.x - boardOriginX) / CELL_SIZE);
        int row = static_cast<int>((mousePos.y - boardOriginY) / CELL_SIZE);

        int idx = drag.selectionIndex;
        gs.selectionPieces[idx].rotation = drag.piece.rotation;

        bool ok = gs.placeSelectionPiece(idx, col, row);
        return {ok, idx, col, row};
    }

    bool isDragging()            const { return drag.active; }
    bool isSpinning()            const { return anim.spinning; }
    const DragState& dragState() const { return drag; }

    // ── Render ───────────────────────────────────────────────────────────────
    void render(sf::RenderWindow& window, const GameState& gs) {
        window.draw(background);

        if (anim.spinning) {
            // Animasyon sırasında dönen tipler göster
            for (int i = 0; i < 3; ++i) {
                Piece tmp;
                tmp.type     = anim.displayTypes[i];
                tmp.rotation = 0;
                float alpha  = 0.5f + 0.5f * std::sin(anim.timer * 20.f + i);
                drawPiecePreview(window, tmp, i, alpha);
                drawSlotBorder(window, i, true);
            }
        } else {
            for (int i = 0; i < SELECTION_SIZE; ++i) {
                bool draggingThis = drag.active && drag.selectionIndex == i;
                float alpha = draggingThis ? 0.3f : 1.0f;
                drawPiecePreview(window, gs.selectionPieces[i], i, alpha);
                drawSlotBorder(window, i, false);

                // Jeton işareti: bu parçayı koysan jeton kazanır mısın?
                // (Düşen parçanın tokeni — selection'da göstermiyoruz burada)
            }
        }

        if (drag.active) drawDraggingPiece(window);

        drawRerollButton(window, gs);
        drawTokenCount(window, gs);
    }

private:
    // ── Reroll ───────────────────────────────────────────────────────────────
    void startSpin(GameState& gs) {
        gs.reroll();
        anim.spinning = true;
        anim.timer    = 0.f;

        gs.freezeFalling(true);
    }

    sf::FloatRect rerollButtonBounds() const {
        // Slot alanının sağında, dikey olarak ortada
        float btnW = 80.f;
        float btnH = 36.f;
        float btnX = originPos.x + width * 0.62f + 10.f;
        float btnY = originPos.y + (BAR_HEIGHT - btnH) * 0.5f;
        return sf::FloatRect{sf::Vector2f{btnX, btnY}, sf::Vector2f{btnW, btnH}};
    }

    void drawRerollButton(sf::RenderWindow& window, const GameState& gs) {
        sf::FloatRect b = rerollButtonBounds();
        bool canSpin = gs.slotMachine.canReroll() && !anim.spinning;

        sf::RectangleShape btn(b.size);
        btn.setPosition(b.position);
        btn.setFillColor(canSpin
            ? sf::Color(60, 180, 80, 220)   // Aktif: yeşil
            : sf::Color(50, 50, 60, 150));   // Pasif: gri
        btn.setOutlineColor(canSpin
            ? sf::Color(100, 255, 120, 240)
            : sf::Color(80, 80, 90, 150));
        btn.setOutlineThickness(1.5f);
        window.draw(btn);

        if (!fontPtr) return;
        std::string label = anim.spinning ? "..." : "CEVİR";
        sf::Text t(*fontPtr, label, 14);
        t.setFillColor(canSpin ? sf::Color::White : sf::Color(120,120,120));
        auto lb = t.getLocalBounds();
        t.setOrigin(sf::Vector2f{lb.size.x * 0.5f, lb.size.y * 0.5f});
        t.setPosition(sf::Vector2f{
            b.position.x + b.size.x * 0.5f,
            b.position.y + b.size.y * 0.5f - lb.size.y * 0.1f});
        window.draw(t);
    }

    void drawTokenCount(sf::RenderWindow& window, const GameState& gs) {
        if (!fontPtr) return;
        float btnX = originPos.x + width * 0.62f + 10.f;
        float btnY = originPos.y + (BAR_HEIGHT - 36.f) * 0.5f;

        // Jeton sayısı butonun altında
        std::string tokenStr = "Jeton: " + std::to_string(gs.slotMachine.tokens);
        sf::Text t(*fontPtr, tokenStr, 12);
        t.setFillColor(sf::Color(220, 200, 80));
        t.setPosition(sf::Vector2f{btnX, btnY + 40.f});
        window.draw(t);
    }

    // ── Slot ve parça çizimi ──────────────────────────────────────────────────
    sf::Vector2f slotCenter(int idx) const {
        float slotW = width * 0.6f / SELECTION_SIZE;
        return {originPos.x + slotW * (idx + 0.5f),
                originPos.y + BAR_HEIGHT * 0.5f};
    }

    int hitTest(sf::Vector2f pos) const {
        if (pos.y < originPos.y || pos.y > originPos.y + BAR_HEIGHT) return -1;
        float slotW = width * 0.6f / SELECTION_SIZE;
        for (int i = 0; i < SELECTION_SIZE; ++i) {
            float x0 = originPos.x + slotW * i;
            if (pos.x >= x0 && pos.x < x0 + slotW) return i;
        }
        return -1;
    }

    void drawPiecePreview(sf::RenderWindow& window, const Piece& piece,
                          int slotIdx, float alpha) {
        sf::Vector2f center = slotCenter(slotIdx);
        int minR=4,maxR=-1,minC=4,maxC=-1;
        for (auto [r,c] : piece.localCells()) {
            minR=std::min(minR,r); maxR=std::max(maxR,r);
            minC=std::min(minC,c); maxC=std::max(maxC,c);
        }
        if (maxR < 0) return;

        // Float'a dönüştürerek C4244'ü çözüyoruz
        float pW = static_cast<float>(maxC - minC + 1) * PIECE_PREVIEW_CELL;
        float pH = static_cast<float>(maxR - minR + 1) * PIECE_PREVIEW_CELL;

        sf::RectangleShape cell(sf::Vector2f{PIECE_PREVIEW_CELL-2.f, PIECE_PREVIEW_CELL-2.f});
        sf::Color c = piece.color();
        c.a = static_cast<std::uint8_t>(alpha * 255);
        cell.setFillColor(c);

        // Uint8_t cast'leri ile C4244'ü çözüyoruz
        cell.setOutlineColor(sf::Color(
            static_cast<std::uint8_t>(std::min(255, int(c.r) + 60)),
            static_cast<std::uint8_t>(std::min(255, int(c.g) + 60)),
            static_cast<std::uint8_t>(std::min(255, int(c.b) + 60)),
            c.a
        ));
        cell.setOutlineThickness(-1.5f);

        float startX = center.x - pW*0.5f;
        float startY = center.y - pH*0.5f;
        for (auto [r,col] : piece.localCells()) {
            cell.setPosition(sf::Vector2f{
                startX + static_cast<float>(col - minC) * PIECE_PREVIEW_CELL + 1.f,
                startY + static_cast<float>(r - minR) * PIECE_PREVIEW_CELL + 1.f});
            window.draw(cell);
        }
    }

    void drawDraggingPiece(sf::RenderWindow& window) {
        const Piece& piece = drag.piece;
        int minR=4,maxR=-1,minC=4,maxC=-1;
        for (auto [r,c] : piece.localCells()) {
            minR=std::min(minR,r); maxR=std::max(maxR,r);
            minC=std::min(minC,c); maxC=std::max(maxC,c);
        }
        if (maxR < 0) return;

        // Float'a dönüştürerek C4244'ü çözüyoruz
        float pW = static_cast<float>(maxC - minC + 1) * CELL_SIZE;
        float pH = static_cast<float>(maxR - minR + 1) * CELL_SIZE;
        float startX = drag.mousePos.x - pW * 0.5f;
        float startY = drag.mousePos.y - pH * 0.5f;

        sf::RectangleShape cell(sf::Vector2f{CELL_SIZE-2.f, CELL_SIZE-2.f});
        sf::Color c = piece.color(); c.a=200;
        cell.setFillColor(c);

        // Uint8_t cast'leri ile C4244'ü çözüyoruz
        cell.setOutlineColor(sf::Color(
            static_cast<std::uint8_t>(std::min(255, int(c.r) + 60)),
            static_cast<std::uint8_t>(std::min(255, int(c.g) + 60)),
            static_cast<std::uint8_t>(std::min(255, int(c.b) + 60)),
            c.a
        ));
        cell.setOutlineThickness(-2.f);

        for (auto [r,col] : piece.localCells()) {
            cell.setPosition(sf::Vector2f{
                startX + static_cast<float>(col - minC) * CELL_SIZE + 1.f,
                startY + static_cast<float>(r - minR) * CELL_SIZE + 1.f});
            window.draw(cell);
        }
    }

    void drawSlotBorder(sf::RenderWindow& window, int idx, bool spinning) {
        float slotW = width * 0.6f / SELECTION_SIZE;
        sf::RectangleShape border(sf::Vector2f{slotW-6.f, BAR_HEIGHT-12.f});
        border.setPosition(sf::Vector2f{originPos.x+slotW*idx+3.f, originPos.y+6.f});
        border.setFillColor(sf::Color::Transparent);
        if (spinning)
            border.setOutlineColor(sf::Color(255,220,50,200));
        else if (drag.active && drag.selectionIndex == idx)
            border.setOutlineColor(sf::Color(255,255,100,200));
        else
            border.setOutlineColor(sf::Color(60,60,100,160));
        border.setOutlineThickness(1.5f);
        window.draw(border);
    }

    sf::Vector2f       originPos;
    float              width;
    sf::RectangleShape background;
    DragState          drag;
    SlotAnimState      anim;
    const sf::Font*    fontPtr { nullptr };
};

} // namespace tetris