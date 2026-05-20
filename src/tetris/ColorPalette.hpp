//
// Created by e-r-e on 5/13/2026.
//

#ifndef TETRIS_COLORPALETTE_HPP
#define TETRIS_COLORPALETTE_HPP

#endif //TETRIS_COLORPALETTE_HPP

#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <array>

namespace tetris {

    struct ColorPalette {
        std::string name;
        sf::Color   background;
        sf::Color   boardBg;
        sf::Color   gridLines;
        sf::Color   panelBg;
        sf::Color   panelBorder;
        sf::Color   pieceI;
        sf::Color   pieceO;
        sf::Color   pieceT;
        sf::Color   pieceS;
        sf::Color   pieceZ;
        sf::Color   pieceJ;
        sf::Color   pieceL;
        sf::Color   ghost;
        sf::Color   token;
        sf::Color   uiAccent;
        sf::Color   uiText;
    };

    inline const std::array<ColorPalette, 4> PALETTES = {{
        // 0 — Carpenter Dark
        {
            "Carpenter Dark",
            sf::Color(0x08, 0x0c, 0x14),
            sf::Color(0x0f, 0x34, 0x60),
            sf::Color(0x0a, 0x7a, 0xbf, 100),
            sf::Color(0x0d, 0x1b, 0x2a),
            sf::Color(0x0a, 0x7a, 0xbf),
            sf::Color(0x00, 0xcf, 0xff),   // I
            sf::Color(0xe0, 0xff, 0x4f),   // O
            sf::Color(0xff, 0x2d, 0x78),   // T
            sf::Color(0x00, 0xcf, 0xff),   // S
            sf::Color(0x0a, 0x7a, 0xbf),   // Z
            sf::Color(0xff, 0x6b, 0x35),   // J
            sf::Color(0xff, 0x2d, 0x78),   // L
            sf::Color(0xff, 0xff, 0xff, 30),
            sf::Color(0xe0, 0xff, 0x4f),
            sf::Color(0x00, 0xcf, 0xff),
            sf::Color(0xe0, 0xff, 0x4f)
        },
        // 1 — Challenger Synth
        {
            "Challenger Synth",
            sf::Color(0x0a, 0x00, 0x10),
            sf::Color(0x2d, 0x00, 0x60),
            sf::Color(0x7b, 0x00, 0xd4, 100),
            sf::Color(0x13, 0x00, 0x28),
            sf::Color(0x7b, 0x00, 0xd4),
            sf::Color(0xd4, 0x00, 0xff),   // I
            sf::Color(0xff, 0xfb, 0xe6),   // O
            sf::Color(0xff, 0x00, 0x90),   // T
            sf::Color(0x00, 0xf5, 0xd4),   // S
            sf::Color(0x7b, 0x00, 0xd4),   // Z
            sf::Color(0xff, 0x00, 0x90),   // J
            sf::Color(0xd4, 0x00, 0xff),   // L
            sf::Color(0xff, 0xff, 0xff, 25),
            sf::Color(0xff, 0xfb, 0xe6),
            sf::Color(0xd4, 0x00, 0xff),
            sf::Color(0x00, 0xf5, 0xd4)
        },
        // 2 — Outrun Grid
        {
            "Outrun Grid",
            sf::Color(0x05, 0x05, 0x0f),
            sf::Color(0x1a, 0x1a, 0x4e),
            sf::Color(0x3d, 0x3d, 0x8f, 120),
            sf::Color(0x0e, 0x0e, 0x2c),
            sf::Color(0x3d, 0x3d, 0x8f),
            sf::Color(0x00, 0xe5, 0xff),   // I
            sf::Color(0xf5, 0xa6, 0x23),   // O
            sf::Color(0xff, 0x20, 0x79),   // T
            sf::Color(0x00, 0xe5, 0xff),   // S
            sf::Color(0xb9, 0x67, 0xff),   // Z
            sf::Color(0xf5, 0xa6, 0x23),   // J
            sf::Color(0xff, 0x20, 0x79),   // L
            sf::Color(0xff, 0xff, 0xff, 28),
            sf::Color(0xf5, 0xa6, 0x23),
            sf::Color(0x00, 0xe5, 0xff),
            sf::Color(0xff, 0x20, 0x79)
        },
        // 3 — Industrial CRT
        {
            "Industrial CRT",
            sf::Color(0x06, 0x06, 0x06),
            sf::Color(0x1a, 0x1a, 0x1a),
            sf::Color(0x2a, 0x2a, 0x2a, 140),
            sf::Color(0x0f, 0x0f, 0x0f),
            sf::Color(0x39, 0xff, 0x14),
            sf::Color(0x39, 0xff, 0x14),   // I
            sf::Color(0xb5, 0xff, 0x3c),   // O
            sf::Color(0xe8, 0xe8, 0xe8),   // T
            sf::Color(0x39, 0xff, 0x14),   // S
            sf::Color(0x55, 0x55, 0x55),   // Z
            sf::Color(0xe8, 0xe8, 0xe8),   // J
            sf::Color(0xb5, 0xff, 0x3c),   // L
            sf::Color(0xff, 0xff, 0xff, 20),
            sf::Color(0xb5, 0xff, 0x3c),
            sf::Color(0x39, 0xff, 0x14),
            sf::Color(0x39, 0xff, 0x14)
        }
    }};

    // Tetromino tipine göre aktif paletten renk çeken yardımcı fonksiyon
    inline sf::Color getPieceColor(TetrominoType type, const ColorPalette& p) {
        switch(type) {
            case TetrominoType::I: return p.pieceI;
            case TetrominoType::O: return p.pieceO;
            case TetrominoType::T: return p.pieceT;
            case TetrominoType::S: return p.pieceS;
            case TetrominoType::Z: return p.pieceZ;
            case TetrominoType::J: return p.pieceJ;
            case TetrominoType::L: return p.pieceL;
            default: return sf::Color::White;
        }
    }

} // namespace tetris
