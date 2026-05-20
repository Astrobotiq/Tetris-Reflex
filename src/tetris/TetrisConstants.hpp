//
// Created by e-r-e on 5/13/2026.
//

#ifndef SIMPLEENGINE2D_TETRISCONSTANTS_HPP
#define SIMPLEENGINE2D_TETRISCONSTANTS_HPP

#endif //SIMPLEENGINE2D_TETRISCONSTANTS_HPP

#pragma once

#include "Board.hpp" // BOARD_COLS, BOARD_ROWS, CELL_SIZE için (sende neredeyse onu include et)

namespace tetris {
    inline constexpr unsigned WIN_W = 700;
    inline constexpr unsigned WIN_H = 740;

    inline constexpr float LEFT_PANEL_W = 140.f;
    inline constexpr float BOARD_X = LEFT_PANEL_W + 10.f;
    inline constexpr float BOARD_Y = 20.f;
    inline constexpr float BOARD_PIXEL_W = BOARD_COLS * CELL_SIZE;
    inline constexpr float BOARD_PIXEL_H = BOARD_ROWS * CELL_SIZE;
    inline constexpr float BAR_Y = BOARD_Y + BOARD_PIXEL_H + 4.f;
}
