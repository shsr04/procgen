#include "sdl_wrap_header.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <_main.hpp>
#include <sdl_wrap.hpp>

using nat = unsigned long long; // C++ guarantees >=64 bits for ULL
using sig = signed long long;

struct color_ident {
    static constexpr pair<SDL_Color, SDL_Color>
        WHITE_ON_BLACK = {{255, 255, 255}, {0, 0, 0}},
        CYAN_ON_BLACK = {{0, 255, 255}, {0, 0, 0}},
        MAGENTA_ON_BLUE = {{255, 0, 255}, {0, 0, 255}},
        GREEN_ON_WHITE = {{0, 255, 0}, {255, 255, 255}},
        RED_ON_BLACK = {{255, 0, 0}, {0, 0, 0}};
    color_ident() = delete;
};

#include "tile.hpp"

#include "coord.hpp"
#include "grid.hpp"

vector<pair<tile::idents, double>> const ROOM_TILE_WEIGHTS = {
    {tile::idents::stone_rubble_pile, 60},
    {tile::idents::stone_flooring, 100},
    {tile::idents::cracked_stone_flooring, 40},
    {tile::idents::decorated_stone_flooring, 30},
    {tile::idents::stone_pillar, 2},
};

grid random_grid(random_gen &rand, sig _size,
                 map<tile::idents, tile> const &tiles) {
    using ti = tile::idents;
    auto size = static_cast<size_t>(_size);

    vector<double> tile_weights;
    for (auto &[t, w] : ROOM_TILE_WEIGHTS)
        tile_weights.push_back(w);
    discrete_distribution<size_t> dist(begin(tile_weights), end(tile_weights));

    vector<vector<ti>> grid(size, vector<ti>(0));
    grid[0] = grid.back() = vector<ti>(size, ti::wall);
    for (auto i_row : nums(1_s, size - 1)) {
        grid[i_row].push_back(ti::wall);
        for (auto i_col : nums(1_s, size - 1)) {
            grid[i_row].push_back(ROOM_TILE_WEIGHTS[dist(rand)].first);
        }
        grid[i_row].push_back(ti::wall);
    }
    return grid;
}

grid empty_grid(sig size) {
    return vector<vector<tile::idents>>(
        size_t(size), vector<tile::idents>(size_t(size), tile::idents::nil));
}

vector<wall_coord> random_wall_coords(random_gen &rand, sig count, sig max_u,
                                      sig min_u) {
    auto door_side = side::LEFT;
    set<wall_coord> r;
    generate_n(inserter(r, begin(r)), count, [&]() {
        auto c = wall_coord(door_side, rand.get(min_u, max_u), max_u);
        door_side = next_side(door_side);
        return c;
    });
    return vector<wall_coord>(begin(r), end(r));
}

vector<plane_coord> random_plane_coords(random_gen &rand, sig count, sig max_xy,
                                        sig min_xy) {
    vector<plane_coord> r;
    generate_n(back_inserter(r), count, [&rand, &max_xy, &min_xy]() {
        return plane_coord{rand.get(min_xy, max_xy), rand.get(min_xy, max_xy),
                           max_xy, min_xy};
    });
    return r;
}

vector<plane_coord> to_plane_coords(vector<wall_coord> p, sig max_xy,
                                    sig min_xy) {
    vector<plane_coord> r;
    for (auto &&w : p) {
        r.push_back(w.to_plane(max_xy, min_xy));
    }
    return r;
}

grid replace_coords(grid grid, vector<plane_coord> coords,
                    tile::idents symbol) {
    for (auto &&c : coords) {
        grid[c] = symbol;
    }
    return grid;
}

pair<vector<plane_coord>, grid> add_random_doorways(random_gen &rand, grid grid,
                                                    sig count) {
    sig max_coord = grid.size() - 1;
    auto doors = to_plane_coords(
        random_wall_coords(rand, count, max_coord - 1, 1), max_coord, 0);
    auto door_sigils = vector<plane_coord>();
    transform(cbegin(doors), cend(doors), back_inserter(door_sigils),
              [&max_coord](auto x) {
                  if (x.x() == 0)
                      ++x.x();
                  else if (x.x() == max_coord)
                      --x.x();
                  if (x.y() == 0)
                      ++x.y();
                  else if (x.y() == max_coord)
                      --x.y();
                  return x;
              });
    auto grid1 = replace_coords(move(grid), doors, tile::idents::doorway);
    auto grid2 =
        replace_coords(move(grid1), door_sigils, tile::idents::doorway_sigil);
    return {move(doors), move(grid2)};
}

bool tile_satisfies_flags(grid const &grid,
                          map<tile::idents, tile> const &tiles,
                          plane_coord const &coord, sig flags) {
    auto tile_flags = static_cast<sig>(tiles.at(grid[coord]).flags);
    return tile_flags & flags;
}

optional<plane_coord> find_adjoining_tile(grid const &grid,
                                          map<tile::idents, tile> const &tiles,
                                          plane_coord const &coord,
                                          tile::idents tile) {
    sig x = coord.x(), y = coord.y();
    for (auto &c : vector<pair<sig, sig>>({{x - 1, y - 1},
                                           {x, y - 1},
                                           {x + 1, y - 1},
                                           {x - 1, y},
                                           {x + 1, y},
                                           {x - 1, y + 1},
                                           {x, y + 1},
                                           {x + 1, y + 1}}))
        if (static_cast<tile::idents>(grid[{c.first, c.second}]) == tile)
            return {{c.first, c.second, grid.size() - 1, 0}};
    return {};
}

string get_description(grid const &grid, map<tile::idents, tile> const &tiles,
                       map<plane_coord, sig> const &out_doors,
                       plane_coord const &coord) {
    auto ident = static_cast<tile::idents>(grid[coord]);
    auto desc = tiles.at(ident).description;
    if (tile_satisfies_flags(grid, tiles, coord, tile::flag_bits::interactable))
        return "(Press e to interact with " + desc + ")";
    if (ident == tile::idents::doorway)
        return "\"Room " + to_string(out_doors.at(coord)) + "\"";
    if (auto o_door =
            find_adjoining_tile(grid, tiles, coord, tile::idents::doorway);
        ident == tile::idents::doorway_sigil && o_door)
        return get_description(grid, tiles, out_doors, *o_door);
    return desc;
}

optional<char> get_tile_symbol(grid const &grid,
                               map<tile::idents, tile> const &tiles,
                               map<plane_coord, sig> const &doors,
                               plane_coord const &coord) {
    if (!tile_satisfies_flags(grid, tiles, coord,
                              tile::flag_bits::shape_changing))
        return tiles.at(grid[coord]).symbol;
    switch (grid[coord]) {
    case tile::idents::doorway_sigil: {
        auto door_coord =
            *find_adjoining_tile(grid, tiles, coord, tile::idents::doorway);
        auto door_num = distance(begin(doors), doors.find(door_coord));
        return 'A' + door_num;
    }
    default:
        return {};
    };
}

struct item {
    enum class idents {
        placeholder,
    };
    string name;
    sig value;
};

map<item::idents, item> ALL_ITEMS = {{item::idents::placeholder, {"***", 10}}};

struct interaction_effect {
    struct effect_bits {
        static sig const none = 0b0, message = 0b1, acquisition = 0b10;
        effect_bits() = delete;
    };
    sig flags;
    optional<string> message;
    optional<item> acquired_item;
};

interaction_effect interact_with(random_gen &rand, grid const &grid,
                                 map<tile::idents, tile> const &tiles,
                                 plane_coord coord) {
    switch (grid[coord]) {
    case tile::idents::chest: {
        using diff_type = decltype(ALL_ITEMS)::difference_type;
        auto pos = ALL_ITEMS.begin();
        item acquired =
            (advance(pos,
                     rand.get(diff_type(0), diff_type(ALL_ITEMS.size() - 1))),
             pos)
                ->second;
        return {.flags = interaction_effect::effect_bits::message |
                         interaction_effect::effect_bits::acquisition,
                .message = "(Obtained " + acquired.name + "!)",
                .acquired_item = acquired};
    }
    case tile::idents::sliding_door:
        return {.flags = interaction_effect::effect_bits::message,
                .message = "(The door is sealed.)"};
    default:
        return {.flags = interaction_effect::effect_bits::none};
    }
}

void print_grid(layers const &layers, map<tile::idents, tile> const &tiles,
                map<plane_coord, sig> const &doors, Window const &win,
                SDL_Rect const &rect, Font const &font) {
    for (auto &grid : layers)
        for (auto &c : grid) {
            if (grid[c] == tile::idents::nil)
                continue;
            auto tile = tiles.at(grid[c]);
            font.renderToSurface(
                string({*get_tile_symbol(grid, tiles, doors, c)}), tile.color,
                win, rect.x + int(c.x()), rect.y + int(c.y()));
        }
}

optional<plane_coord> display_room(random_gen rand, layers grid,
                                   map<plane_coord, sig> const &out_doors,
                                   Window const &main_win,
                                   SDL_Rect const &room_view,
                                   SDL_Rect const &info_view, Font const &font,
                                   plane_coord player_pos, string room_title) {
    auto interaction_point = optional<plane_coord>();
    grid[1][player_pos] = tile::idents::player;
    main_win.clear({0, 0, 0});
    print_grid(grid, ALL_TILES, out_doors, main_win, room_view, font);
    font.renderToSurface("--- " + room_title + "---",
                         color_ident::WHITE_ON_BLACK, main_win, info_view.x,
                         info_view.y);
    main_win.updateWindow();
    while (true) {
        SDL_Event event;
        auto c = 0;
        while (SDL_PollEvent(&event)) {
            if (event.type != SDL_KEYDOWN)
                continue;
            c = event.key.keysym.sym;
            break;
        }
        auto prev = player_pos;
        if (c == SDLK_w)
            --player_pos.y();
        else if (c == SDLK_s)
            ++player_pos.y();
        else if (c == SDLK_a)
            --player_pos.x();
        else if (c == SDLK_d)
            ++player_pos.x();
        else if (c == SDLK_e && interaction_point) {
            main_win.clear({0, 0, 0});
            print_grid(grid, ALL_TILES, out_doors, main_win, room_view, font);
            auto effect =
                interact_with(rand, grid[0], ALL_TILES, *interaction_point);
            font.renderToSurface(*effect.message, color_ident::WHITE_ON_BLACK,
                                 main_win, info_view.x, info_view.y);
            main_win.updateWindow();
            continue;
        } else if (c == SDLK_q)
            return {};

        if (player_pos == prev)
            continue;

        if (tile_satisfies_flags(grid[0], ALL_TILES, player_pos,
                                 tile::flag_bits::interactable))
            interaction_point = player_pos;
        else
            interaction_point = {};
        if (!tile_satisfies_flags(grid[0], ALL_TILES, player_pos,
                                  tile::flag_bits::passable)) {
            player_pos = prev;
            if (!interaction_point)
                continue;
        }
        if (tile_satisfies_flags(grid[0], ALL_TILES, player_pos,
                                 tile::flag_bits::transporting)) {
            return player_pos;
        }

        grid[1][prev] = tile::idents::nil;
        grid[1][player_pos] = tile::idents::player;

        main_win.clear({0, 0, 0});
        print_grid(grid, ALL_TILES, out_doors, main_win, room_view, font);
        auto described_coord =
            interaction_point ? *interaction_point : player_pos;
        auto descr =
            get_description(grid[0], ALL_TILES, out_doors, described_coord);
        font.renderToSurface(descr, color_ident::WHITE_ON_BLACK, main_win,
                             info_view.x, info_view.y);
        main_win.updateWindow();
    }
}

pair<layers, vector<plane_coord>> build_room(random_gen &rand, sig grid_size) {
    auto chest_coords = random_plane_coords(
        rand, static_cast<sig>(rand.get(0, 2)), grid_size - 2, 1);
    auto &&[door_coords, bottom_grid] =
        add_random_doorways(rand, random_grid(rand, grid_size, ALL_TILES),
                            static_cast<sig>(rand.get(2, 6)));
    return {layers{replace_coords(move(bottom_grid), chest_coords,
                                  tile::idents::chest),
                   empty_grid(grid_size)},
            move(door_coords)};
}

int main() {
    auto const window_size = 60;
    Init _init(SDL_INIT_VIDEO);
    assert_true(TTF_Init() == 0, "TTF init failed");
    Font font("NotoMono-Regular.ttf", 24);
    auto const font_size = font.height();
    Window main_win(font_size * window_size / 2, font_size * window_size / 2,
                    "Hello", SDL_WINDOW_INPUT_FOCUS);

    auto seed = random_device()();
    auto room_id = sig(0);
    auto latest_visited_room = optional<sig>();
    auto next_free_room = sig(1);
    map<sig, map<plane_coord, sig>> room_network;
    while (true) {
        room_network[room_id] = {};
        random_gen rand(seed + static_cast<decltype(seed)>(room_id));
        auto const grid_size =
            sig(rand.get(window_size / 4, window_size / 3) + 5);
        /// ATTENTION: The rects should only be accessed via
        /// font.renderToSurface()! Otherwise the actual pixel size of the font
        /// has to be considered when drawing something "by hand". That means: a
        /// rect of size [w,h] corresponds to an area of [w*f,h*f] pixels where
        /// f is the main font size.
        auto room_view =
            SDL_Rect{.x = 0, .y = 0, .w = int(grid_size), .h = int(grid_size)};
        auto info_view =
            SDL_Rect{.x = 0, .y = room_view.h + 1, .w = window_size, .h = 1};

        main_win.updateWindow();
        auto &&[grid, doors] = build_room(rand, grid_size);
        auto player_pos = plane_coord(grid[0].size() / 2, grid[0].size() / 2,
                                      grid[0].size() - 1, 0);
        if (latest_visited_room) {
            room_network[room_id][doors.back()] = *latest_visited_room;
            grid[0][doors.back()] = tile::idents::sliding_door;
            player_pos = *find_adjoining_tile(grid[0], ALL_TILES, doors.back(),
                                              tile::idents::doorway_sigil);
            doors.pop_back();
        }
        for (auto &&c_door : move(doors)) {
            room_network.at(room_id)[c_door] = next_free_room;
            next_free_room++;
        }
        auto exit_door = display_room(rand, grid, room_network[room_id],
                                      main_win, room_view, info_view, font,
                                      player_pos, "Room " + to_string(room_id));
        if (!exit_door)
            break;
        latest_visited_room = room_id;
        room_id = room_network.at(room_id).at(*exit_door);
    }
}
