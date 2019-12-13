
struct tile {
    struct flag_bits {
        static sig const none = 0b0, passable = 0b1, interactable = 0b10,
                         transporting = 0b100;
        flag_bits() = delete;
    };
    enum class idents : char {
        wall = '#',
        doorway = ' ',
        doorway_sigil = 'D',
        chest = '?',
        stone_rubble_pile = '"',
        stone_flooring = '.',
        cracked_stone_flooring = ',',
        decorated_stone_flooring = '~',
        stone_pillar = 'O',
        player = '*',
    };
    char symbol;
    sig flags;
    string description;
    color_ident color = WHITE_ON_BLACK;
    nc::chtype attr = A_NORMAL;
};

map<tile::idents, tile> const all_tiles = {
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
         CYAN_ON_BLACK,
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
         tile::flag_bits::passable,
     }},
    {tile::idents::chest,
     {
         '?',
         tile::flag_bits::interactable,
         "mysterious chest",
         MAGENTA_ON_BLUE,
     }},
    {tile::idents::player,
     {
         '*',
         tile::flag_bits::none,
         "Player character",
         GREEN_ON_WHITE,
     }},
};
