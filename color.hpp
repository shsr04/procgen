#pragma once
#include <SDL2/SDL_pixels.h>
#include <_main.hpp>

struct color_idents {
    static constexpr pair<SDL_Color, SDL_Color>
        WHITE_ON_BLACK = {{255, 255, 255}, {0, 0, 0}},
        GRAY_ON_BLACK = {{127, 127, 127}, {0, 0, 0}},
        CYAN_ON_BLACK = {{0, 255, 255}, {0, 0, 0}},
        MAGENTA_ON_BLUE = {{255, 0, 255}, {0, 0, 255}},
        GREEN_ON_WHITE = {{0, 255, 0}, {255, 255, 255}},
        RED_ON_BLACK = {{255, 0, 0}, {0, 0, 0}},
        ORANGE_ON_BLACK = {{255, 127, 127}, {0, 0, 0}};
    color_idents() = delete;
};
