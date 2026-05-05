#pragma once

#include "Board.hpp"
#include "Tetromino.hpp"
#include <random>
#include <array>
#include <optional>
#include <functional>
#include <climits>

namespace tetris {

inline constexpr int SELECTION_SIZE = 3;
inline constexpr std::array<int, 5> LINE_SCORES = {0, 100, 300, 500, 800};

// ─────────────────────────────────────────────────────────────────────────────
// BoardAnalysis  —  Board'un mevcut durumunu sayısal olarak özetler.
// Her seçim parçası üretiminden önce hesaplanır.
// ─────────────────────────────────────────────────────────────────────────────
struct BoardAnalysis {
    std::array<int, BOARD_COLS> colHeights{};
    float averageHeight      { 0.f };
    int   bumpiness          { 0   };  // Komşu sütun yükseklik farkları toplamı
    int   holes              { 0   };  // Dolu bloğun altındaki boş hücreler
    int   emptiest_col_start { 0   };  // En boş 3-sütunluk bölge başlangıcı
    int   emptiest_col_end   { BOARD_COLS - 1 };
};

// ─────────────────────────────────────────────────────────────────────────────
struct GameState {
    Board board;
    Piece fallingPiece;
    bool  gameOver            { false };
    bool  paused              { false };
    bool  debugDisableGravity { false };

    std::array<Piece, SELECTION_SIZE> selectionPieces;

    int   m_score        { 0    };
    int   linesCleared { 0    };
    int   level        { 1    };
    float fallInterval { 1.0f };
    float fallAccum    { 0.f  };

    std::array<Piece, 3> nextPieces;

    std::function<void(int, std::vector<int>)> onLinesCleared;
    std::function<void()>                      onPieceLocked;
    std::function<void()>                      onGameOver;

    explicit GameState(std::uint32_t seed = 42)
        : m_rng(seed)
    {
        for (auto& p : nextPieces)    p = spawnPieceRandom();
        refreshSelectionPieces();
        spawnFallingPiece();
    }

    // ── Güncelleme ───────────────────────────────────────────────────────────
    void update(float dt) {
        if (gameOver || paused) return;
        if (!debugDisableGravity) {
            fallAccum += dt;
            if (fallAccum >= fallInterval) {
                fallAccum -= fallInterval;
                stepFallingPiece();
            }
        }
    }

    // ── Seçim parçasını yerleştir ────────────────────────────────────────────
    // targetRow parametresi korundu (board.getDirectPlacement ile uyumlu)
    bool placeSelectionPiece(int selectionIndex, int targetCol, int targetRow) {
        if (selectionIndex < 0 || selectionIndex >= SELECTION_SIZE) return false;

        Piece piece = selectionPieces[selectionIndex];
        auto [valid, placed] = board.getDirectPlacement(piece, targetCol, targetRow);
        if (!valid) return false;

        board.place(placed);
        processLineClears();

        // Board değişti — tüm slotları board'a göre yenile
        refreshSelectionPieces();

        //hardDropFallingPiece();
        return true;
    }

    void rotateSelectionPiece(int index) {
        if (index >= 0 && index < SELECTION_SIZE)
            selectionPieces[index].rotateRight();
    }

    void recalcLevel() {
        level = std::max(1, linesCleared / 10 + 1);
        fallInterval = std::max(0.1f, 1.0f - (level - 1) * 0.08f);
    }

private:
    std::mt19937 m_rng;

    // ── Slot kalitesi ────────────────────────────────────────────────────────
    // Slot 0 → %90 board-aware (garantili iyi parça)
    // Slot 1 → %50 board-aware (orta)
    // Slot 2 → %0  rastgele   (sürpriz / zorluk)
    float slotQuality(int slotIndex) const {
        static constexpr std::array<float, SELECTION_SIZE> q = { 0.9f, 0.5f, 0.0f };
        return q[slotIndex];
    }

    // ── Board analizi ────────────────────────────────────────────────────────
    BoardAnalysis analyzeBoard() const {
        BoardAnalysis a;

        // Sütun yükseklikleri: yukarıdan tara, ilk dolu hücreyi bul
        for (int c = 0; c < BOARD_COLS; ++c) {
            a.colHeights[c] = 0;
            for (int r = 0; r < BOARD_ROWS; ++r) {
                if (!board.isEmpty(c, r)) {
                    a.colHeights[c] = BOARD_ROWS - r;
                    break;
                }
            }
        }

        // Ortalama yükseklik
        int total = 0;
        for (int h : a.colHeights) total += h;
        a.averageHeight = static_cast<float>(total) / BOARD_COLS;

        // Bumpiness: komşu sütun yükseklik farklarının toplamı
        a.bumpiness = 0;
        for (int c = 0; c < BOARD_COLS - 1; ++c)
            a.bumpiness += std::abs(a.colHeights[c] - a.colHeights[c + 1]);

        // Kapalı boşluklar: dolu bloğun altındaki boş hücreler
        a.holes = 0;
        for (int c = 0; c < BOARD_COLS; ++c) {
            bool blockAbove = false;
            for (int r = 0; r < BOARD_ROWS; ++r) {
                if (!board.isEmpty(c, r)) blockAbove = true;
                else if (blockAbove)      a.holes++;
            }
        }

        // En boş 3-sütunluk pencere
        int minSum = INT_MAX;
        for (int c = 0; c <= BOARD_COLS - 3; ++c) {
            int sum = a.colHeights[c] + a.colHeights[c+1] + a.colHeights[c+2];
            if (sum < minSum) {
                minSum = sum;
                a.emptiest_col_start = c;
                a.emptiest_col_end   = c + 2;
            }
        }

        return a;
    }

    // ── Parça skoru ──────────────────────────────────────────────────────────
    // Bu parça tipi + dönüş, bu board'a ne kadar uyuyor?
    float scorePiece(TetrominoType type, int rotation,
                     const BoardAnalysis& a) const
    {
        Piece tmp;
        tmp.type     = type;
        tmp.rotation = rotation;
        tmp.col      = 0;
        tmp.row      = 0;
        auto cells = tmp.localCells();

        int minC = INT_MAX, maxC = INT_MIN;
        int minR = INT_MAX, maxR = INT_MIN;
        for (auto [r, c] : cells) {
            minC = std::min(minC, c); maxC = std::max(maxC, c);
            minR = std::min(minR, r); maxR = std::max(maxR, r);
        }
        int pieceWidth  = (minC == INT_MAX) ? 1 : (maxC - minC + 1);
        int pieceHeight = (minR == INT_MAX) ? 1 : (maxR - minR + 1);

        float score = 0.f;

        // Kural 1: Düz tahta → geniş yatay parçalar
        if (a.bumpiness <= 4) {
            if (type == TetrominoType::I && rotation % 2 == 0) score += 40.f;
            if (type == TetrominoType::O)                       score += 20.f;
        }

        // Kural 2: Engebeli tahta → T, S, Z
        if (a.bumpiness > 6) {
            if (type == TetrominoType::T) score += 25.f;
            if (type == TetrominoType::S) score += 15.f;
            if (type == TetrominoType::Z) score += 15.f;
        }

        // Kural 3: Çok yüksek sütun varsa → dikey I veya J/L
        for (int c = 0; c < BOARD_COLS; ++c) {
            if (a.colHeights[c] > a.averageHeight + 4.f) {
                if (type == TetrominoType::I && rotation % 2 == 1) score += 35.f;
                if (type == TetrominoType::J || type == TetrominoType::L) score += 15.f;
            }
        }

        // Kural 4: En boş bölgenin genişliğine uygun parça
        int emptyWidth = a.emptiest_col_end - a.emptiest_col_start + 1;
        if (pieceWidth <= emptyWidth) score += 20.f;

        // Kural 5: Kapalı boşluk çoksa geniş parçaları cezalandır
        if (a.holes > 3 && pieceWidth >= 3) score -= 10.f;

        // Kural 6: Kritik yükseklikte → küçük parçalar tercih et
        if (a.averageHeight > BOARD_ROWS * 0.6f) {
            score += (4 - pieceWidth)  * 10.f;
            score += (4 - pieceHeight) * 5.f;
        }

        return score;
    }

    // ── Board-aware parça üretimi ────────────────────────────────────────────
    // quality=0.0 → tamamen rastgele
    // quality=1.0 → her zaman en iyi skor alan parça
    Piece spawnPieceAware(float quality) {
        if (quality <= 0.f) return spawnPieceRandom();

        BoardAnalysis analysis = analyzeBoard();

        TetrominoType bestType     = TetrominoType::I;
        int           bestRotation = 0;
        float         bestScore    = -1.f;

        int typeCount = static_cast<int>(TetrominoType::COUNT);
        for (int t = 0; t < typeCount; ++t) {
            for (int r = 0; r < 4; ++r) {
                float s = scorePiece(static_cast<TetrominoType>(t), r, analysis);
                if (s > bestScore) {
                    bestScore    = s;
                    bestType     = static_cast<TetrominoType>(t);
                    bestRotation = r;
                }
            }
        }

        std::uniform_real_distribution<float> chance(0.f, 1.f);
        Piece result;
        if (chance(m_rng) < quality) {
            result.type     = bestType;
            result.rotation = bestRotation;
        } else {
            result.type     = randomType(m_rng);
            std::uniform_int_distribution<int> rotDist(0, 3);
            result.rotation = rotDist(m_rng);
        }
        result.col = BOARD_COLS / 2 - 2;
        result.row = 0;
        return result;
    }

    // Tamamen rastgele parça — orijinal davranış
    Piece spawnPieceRandom() {
        Piece p;
        p.type = randomType(m_rng);
        std::uniform_int_distribution<int> rotDist(0, 3);
        p.rotation = rotDist(m_rng);
        p.col = BOARD_COLS / 2 - 2;
        p.row = 0;
        return p;
    }

    // Tüm seçim slotlarını board durumuna göre yenile
    void refreshSelectionPieces() {
        for (int i = 0; i < SELECTION_SIZE; ++i)
            selectionPieces[i] = spawnPieceAware(slotQuality(i));
    }

    // ── Düşen parça için akıllı kolon seçimi ─────────────────────────────────
    // %70 board'un en boş bölgesine, %30 rastgele
    int chooseFallingCol(const Piece& piece) {
        BoardAnalysis a = analyzeBoard();

        int minC = INT_MAX, maxC = INT_MIN;
        for (auto [r, c] : piece.localCells()) {
            minC = std::min(minC, c);
            maxC = std::max(maxC, c);
        }
        int pieceWidth = (minC == INT_MAX) ? 1 : (maxC - minC + 1);
        int maxCol     = BOARD_COLS - pieceWidth;

        int targetCol = a.emptiest_col_start - minC;
        targetCol = std::clamp(targetCol, 0, maxCol);

        std::uniform_real_distribution<float> chance(0.f, 1.f);
        if (chance(m_rng) < 0.7f) {
            // Board-aware: hedef kolona ±1 jitter ekle (aynı noktaya yığılmasın)
            std::uniform_int_distribution<int> jitter(-1, 1);
            return std::clamp(targetCol + jitter(m_rng), 0, maxCol);
        } else {
            std::uniform_int_distribution<int> colDist(0, maxCol);
            return colDist(m_rng);
        }
    }

    void spawnFallingPiece() {
        fallingPiece     = nextPieces[0];
        fallingPiece.row = 0;
        fallingPiece.col = chooseFallingCol(fallingPiece);

        nextPieces[0] = nextPieces[1];
        nextPieces[1] = nextPieces[2];
        nextPieces[2] = spawnPieceRandom();

        if (!board.canPlace(fallingPiece)) {
            gameOver = true;
            if (onGameOver) onGameOver();
        }
    }

    void stepFallingPiece() {
        Piece next = fallingPiece;
        next.row++;
        if (board.canPlace(next)) {
            fallingPiece = next;
        } else {
            board.place(fallingPiece);
            if (onPieceLocked) onPieceLocked();
            processLineClears();
            spawnFallingPiece();
        }
    }

    void hardDropFallingPiece() {
        Piece next = fallingPiece;
        while (true) {
            next.row++;
            if (!board.canPlace(next)) break;
            fallingPiece = next;
        }
        board.place(fallingPiece);
        if (onPieceLocked) onPieceLocked();
        processLineClears();
        spawnFallingPiece();
        fallAccum = 0.f;
    }

    void processLineClears() {
        std::vector<int> clearedRows;
        for (int r = 0; r < BOARD_ROWS; ++r)
            if (board.isRowFull(r)) clearedRows.push_back(r);

        int n = board.clearFullRows();
        if (n > 0) {
            int pts = LINE_SCORES[std::min(n, 4)] * level;
            m_score        += pts;
            linesCleared += n;
            recalcLevel();
            if (onLinesCleared) onLinesCleared(n, clearedRows);
        }
    }
};

} // namespace tetris