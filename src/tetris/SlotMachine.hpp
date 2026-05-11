#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SlotMachine.hpp  —  Jeton sistemi, reroll ve güç mekaniği
//
// AKIŞ:
//   1. Bazı düşen parçalar "jeton içerir" (hasToken = true)
//   2. O parçanın dahil olduğu satır temizlenirse jeton kazanılır
//   3. Yeterli jeton varsa "ÇEVİR" butonu aktif olur
//   4. ÇEVİR: 3 slot yenilenir, kombo kontrolü yapılır
//   5. 2 aynı = küçük güç, 3 aynı = büyük güç
//   6. Kazanılan güçler PowerSlot listesinde birikir
// ─────────────────────────────────────────────────────────────────────────────

#include "Tetromino.hpp"
#include <array>
#include <string>
#include <vector>

namespace tetris {

// ─────────────────────────────────────────────────────────────────────────────
// PowerType  —  Kullanılabilir güç tipleri
// ─────────────────────────────────────────────────────────────────────────────
enum class PowerType {
    // I kombinasyonları
    I_Small,   // 2×I → seçilen 1 satırı temizle
    I_Big,     // 3×I → seçilen 2 satırı temizle

    // O kombinasyonları
    O_Small,   // 2×O → Zamanı 4 saniye durdur
    O_Big,     // 3×O → Zamanı 8 saniye durdur

    // T kombinasyonları
    T_Big,     // 3×T → istediğin parçayı seç ve yerleştir

    // L kombinasyonları
    L_Small,   // 2×L → düşen parçayı 1× sola döndür
    L_Big,     // 3×L → düşen parçayı 2× sola döndür

    // J kombinasyonları
    J_Small,   // 2×J → düşen parçayı 1× sağa döndür
    J_Big,     // 3×J → düşen parçayı 2× sağa döndür

    // S kombinasyonları
    S_Small,   // 2×S → en yüksek sütunu 1 hücre indir
    S_Big,     // 3×S → en yüksek 2 sütunu 1 hücre indir

    // Z kombinasyonları
    Z_Small,   // 2×Z → rastgele 1 bloğu patlat
    Z_Big,     // 3×Z → rastgele 3 bloğu patlat

    // Özel: S+Z (slot sonucunda değil, board_aware kontrolde)
    SZ_Special, // En yüksek sütunu 2 hücre indir

    None
};

// ─────────────────────────────────────────────────────────────────────────────
// PowerSlot  —  Kazanılmış bir gücü temsil eder
// ─────────────────────────────────────────────────────────────────────────────
struct PowerSlot {
    PowerType   type  { PowerType::None };
    bool        used  { false };

    std::string name() const {
        switch (type) {
            case PowerType::I_Small:    return "Ix2: Clear 1 Row";
            case PowerType::I_Big:      return "Ix3: Clear 2 Rows";
            case PowerType::O_Small:    return "Ox2: Freeze 4s";
            case PowerType::O_Big:      return "Ox3: Freeze 8s";
            case PowerType::T_Big:      return "Tx3: Pick Any Piece";
            case PowerType::L_Small:    return "Lx2: Rotate Left";
            case PowerType::L_Big:      return "Lx3: 2x Rotate Left";
            case PowerType::J_Small:    return "Jx2: Rotate Right";
            case PowerType::J_Big:      return "Jx3: 2x Rotate Right";
            case PowerType::S_Small:    return "Sx2: Column -1";
            case PowerType::S_Big:      return "Sx3: 2 Columns -1";
            case PowerType::Z_Small:    return "Zx2: Destroy 1 Block";
            case PowerType::Z_Big:      return "Zx3: Destroy 3 Blocks";
            case PowerType::SZ_Special: return "S+Z: Column -2";
            default:                    return "?";
        }
    }

    sf::Color color() const {
        switch (type) {
            case PowerType::I_Small: case PowerType::I_Big:
                return sf::Color(0, 240, 240);
            case PowerType::O_Small: case PowerType::O_Big:
                return sf::Color(240, 240, 0);
            case PowerType::T_Big:
                return sf::Color(160, 0, 240);
            case PowerType::L_Small: case PowerType::L_Big:
                return sf::Color(240, 160, 0);
            case PowerType::J_Small: case PowerType::J_Big:
                return sf::Color(0, 0, 240);
            case PowerType::S_Small: case PowerType::S_Big:
                return sf::Color(0, 240, 0);
            case PowerType::Z_Small: case PowerType::Z_Big:
                return sf::Color(240, 0, 0);
            case PowerType::SZ_Special:
                return sf::Color(200, 100, 255);
            default:
                return sf::Color(120, 120, 120);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SlotResult  —  Çevirme sonucu
// ─────────────────────────────────────────────────────────────────────────────
struct SlotResult {
    std::array<TetrominoType, 3> types {};
    PowerType                    power { PowerType::None };
    bool                         hasCombo { false };
    int                          comboCount { 0 };  // 2 veya 3
};

// ─────────────────────────────────────────────────────────────────────────────
// SlotMachine  —  Jeton ve güç yönetimi
// ─────────────────────────────────────────────────────────────────────────────
class SlotMachine {
public:
    static constexpr int REROLL_COST   = 1;   // Reroll için gereken jeton
    static constexpr int MAX_POWERS    = 5;   // Aynı anda tutulabilecek max güç
    static constexpr int TOKEN_CHANCE  = 30;  // Düşen parçada jeton olma şansı (%)

    int tokens { 0 };
    std::vector<PowerSlot> powers;

    // Jeton ekle
    void addToken(int amount = 1) {
        tokens = std::min(tokens + amount, 9); // Max 9 jeton
    }

    // Reroll yapılabilir mi?
    bool canReroll() const {
        return tokens >= REROLL_COST;
    }

    // 3 slot için kombo kontrolü — güç belirle
    SlotResult evaluateCombo(const std::array<TetrominoType, 3>& types) const {
        SlotResult result;
        result.types = types;

        // Kaç tane eşleşme var?
        int countA = (types[0] == types[1] ? 1 : 0)
                   + (types[1] == types[2] ? 1 : 0)
                   + (types[0] == types[2] ? 1 : 0);

        if (countA == 0) {
            // Özel: S + Z kombinasyonu
            bool hasS = (types[0] == TetrominoType::S ||
                         types[1] == TetrominoType::S ||
                         types[2] == TetrominoType::S);
            bool hasZ = (types[0] == TetrominoType::Z ||
                         types[1] == TetrominoType::Z ||
                         types[2] == TetrominoType::Z);
            if (hasS && hasZ) {
                result.hasCombo   = true;
                result.comboCount = 2; // Özel kombo — small olarak işle
                result.power      = PowerType::SZ_Special;
            }
            return result;
        }

        // En az 2 eşleşen tipi bul
        TetrominoType matchType = types[0];
        int matchCount = 1;
        if (types[1] == types[0]) { matchCount++; }
        if (types[2] == types[0]) { matchCount++; }
        if (matchCount == 1) { matchType = types[1]; matchCount = 2; } // 1-2 eşleşti

        // 3 aynı mı?
        if (types[0] == types[1] && types[1] == types[2]) {
            matchCount = 3;
            matchType  = types[0];
        }

        result.hasCombo   = true;
        result.comboCount = matchCount;
        result.power      = getPower(matchType, matchCount);
        return result;
    }

    // Güç kazandır
    void awardPower(PowerType p) {
        if (p == PowerType::None) return;
        if (static_cast<int>(powers.size()) >= MAX_POWERS) return;
        PowerSlot slot;
        slot.type = p;
        powers.push_back(slot);
    }

    // Güç kullan (index ile)
    bool usePower(int index) {
        if (index < 0 || index >= static_cast<int>(powers.size())) return false;
        if (powers[index].used) return false;
        powers[index].used = true;
        return true;
    }

    // Kullanılmış güçleri temizle
    void cleanUsedPowers() {
        powers.erase(
            std::remove_if(powers.begin(), powers.end(),
                [](const PowerSlot& p){ return p.used; }),
            powers.end());
    }

    // Aktif (kullanılmamış) güç var mı?
    bool hasActivePower() const {
        for (auto& p : powers)
            if (!p.used) return true;
        return false;
    }

private:
    // Şekil + kombinasyon sayısına göre güç döndür
    PowerType getPower(TetrominoType type, int count) const {
        bool big = (count >= 3);
        switch (type) {
            case TetrominoType::I: return big ? PowerType::I_Big : PowerType::I_Small;
            case TetrominoType::O: return big ? PowerType::O_Big : PowerType::O_Small;
            case TetrominoType::T: return PowerType::T_Big;
            case TetrominoType::L: return big ? PowerType::L_Big : PowerType::L_Small;
            case TetrominoType::J: return big ? PowerType::J_Big : PowerType::J_Small;
            case TetrominoType::S: return big ? PowerType::S_Big : PowerType::S_Small;
            case TetrominoType::Z: return big ? PowerType::Z_Big : PowerType::Z_Small;
            default:               return PowerType::None;
        }
    }
};

} // namespace tetris