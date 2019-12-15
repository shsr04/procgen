#pragma once
#include "color.hpp"
#include <_main.hpp>

struct tile {
    struct flag_bits {
        static sig const none = 0b0, passable = 0b1, interactable = 0b10,
                         transporting = 0b100, shape_changing = 0b1000,
                         damaging = 1 << 4;
        flag_bits() = delete;
    };
    struct attr_bits {
        static sig const none = 0b0;
        attr_bits() = delete;
    };
    enum class idents : char {
        nil = 0,
        wall,
        doorway,
        doorway_sigil,
        sliding_door,
        chest,
        stone_rubble_pile,
        stone_flooring,
        cracked_stone_flooring,
        decorated_stone_flooring,
        stone_pillar,
        player,
        dart_trap,
        propelled_dart,
    };
    char symbol;
    sig flags;
    string description;
    pair<SDL_Color, SDL_Color> color = color_idents::WHITE_ON_BLACK;
    sig attr = attr_bits::none;
};

vector<pair<tile::idents, double>> const ROOM_TILE_WEIGHTS = {
    {tile::idents::stone_rubble_pile, 60},
    {tile::idents::stone_flooring, 100},
    {tile::idents::cracked_stone_flooring, 40},
    {tile::idents::decorated_stone_flooring, 30},
    {tile::idents::stone_pillar, 2},
    {tile::idents::dart_trap, 2},
};

map<tile::idents, tile> const ALL_TILES = {
    {tile::idents::stone_rubble_pile,
     {
         '"',
         tile::flag_bits::passable,
         "small pile of rubble",
     }},
    {tile::idents::stone_flooring,
     {
         '.',
         tile::flag_bits::passable,
         "stone floor",
     }},
    {tile::idents::cracked_stone_flooring,
     {
         ',',
         tile::flag_bits::passable,
         "cracked stone floor",
     }},
    {tile::idents::decorated_stone_flooring,
     {
         '~',
         tile::flag_bits::passable,
         "decorated stone floor",
     }},
    {tile::idents::stone_pillar,
     {
         'O',
         tile::flag_bits::none,
         "stone pillar",
         color_idents::CYAN_ON_BLACK,
     }},
    {tile::idents::wall,
     {
         '#',
         tile::flag_bits::none,
     }},
    {tile::idents::doorway,
     {
         ' ',
         tile::flag_bits::passable | tile::flag_bits::transporting,
     }},
    {tile::idents::doorway_sigil,
     {
         'D',
         tile::flag_bits::passable | tile::flag_bits::shape_changing,
         "",
         color_idents::RED_ON_BLACK,
     }},
    {tile::idents::dart_trap,
     {
         '#',
         tile::flag_bits::none,
         "A devious trap",
         color_idents::WHITE_ON_BLACK,
     }},
    {tile::idents::sliding_door,
     {
         '#',
         tile::flag_bits::interactable,
         "sliding door",
         color_idents::RED_ON_BLACK,
     }},
    {tile::idents::chest,
     {
         '?',
         tile::flag_bits::interactable,
         "mysterious chest",
         color_idents::MAGENTA_ON_BLUE,
     }},
    {tile::idents::player,
     {
         '*',
         tile::flag_bits::none,
         "Player character",
         color_idents::GREEN_ON_WHITE,
     }},
    {tile::idents::propelled_dart,
     {
         '`',
         tile::flag_bits::damaging,
         "",
         color_idents::RED_ON_BLACK,
     }},
};
