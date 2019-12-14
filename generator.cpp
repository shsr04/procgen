#include <_main.hpp>
namespace nc {
extern "C" {
#include <curses.h>
}
} // namespace nc

using nat = unsigned long long; // C++ guarantees >=64 bits for ULL
using sig = signed long long;

enum color_ident : short {
    WHITE_ON_BLACK = 0,
    CYAN_ON_BLACK = 1,
    MAGENTA_ON_BLUE = 2,
    GREEN_ON_WHITE = 3,
    RED_ON_BLACK = 4,
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
    auto i=nums(1,10); 
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

void interact_with(grid const &grid, map<tile::idents, tile> const &tiles,
                   plane_coord coord, nc::WINDOW *win) {
    switch (grid[coord]) {
    case tile::idents::chest:
        nc::wclear(win);
        nc::mvwprintw(win, 0, 0, "(Obtained ***!)");
        nc::wrefresh(win);
        break;
    case tile::idents::sliding_door:
        nc::wclear(win);
        nc::mvwprintw(win, 0, 0, "(The door is sealed.)");
        nc::wrefresh(win);
        break;
    default:
        return;
    }
}

void print_grid(layers const &layers, map<tile::idents, tile> const &tiles,
                map<plane_coord, sig> const &doors, nc::WINDOW *win) {
    for (auto &grid : layers)
        for (auto &c : grid) {
            if (grid[c] == tile::idents::nil)
                continue;
            auto tile = tiles.at(grid[c]);
            nc::wattr_set(win, tile.attr, tile.color, NULL);
            mvwaddch(win, c.y(), c.x(),
                     *get_tile_symbol(grid, tiles, doors, c));
        }
}

optional<plane_coord> display_room(layers grid,
                                   map<plane_coord, sig> const &out_doors,
                                   nc::WINDOW *main_win, nc::WINDOW *room_view,
                                   nc::WINDOW *info_view,
                                   plane_coord player_pos, string room_title) {
    auto interaction_point = optional<plane_coord>();
    grid[1][player_pos] = tile::idents::player;
    print_grid(grid, ALL_TILES, out_doors, room_view);
    nc::mvwprintw(info_view, 0, 0, ("--- " + room_title + "---").c_str());
    nc::wrefresh(main_win);
    while (true) {
        auto c = nc::wgetch(main_win);
        auto prev = player_pos;
        if (c == 'w')
            --player_pos.y();
        else if (c == 's')
            ++player_pos.y();
        else if (c == 'a')
            --player_pos.x();
        else if (c == 'd')
            ++player_pos.x();
        else if (c == 'e' && interaction_point) {
            interact_with(grid[0], ALL_TILES, *interaction_point, info_view);
            continue;
        } else if (c == 'q')
            return {};
        else if (c == KEY_RESIZE)
            nc::wrefresh(main_win);

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

        nc::wclear(main_win);
        print_grid(grid, ALL_TILES, out_doors, room_view);
        auto described_coord =
            interaction_point ? *interaction_point : player_pos;
        auto descr =
            get_description(grid[0], ALL_TILES, out_doors, described_coord);
        mvwprintw(info_view, 0, 0, descr.c_str());
        // nc::wmove(room_view, int(pos.y()), int(pos.x()));
        nc::wrefresh(main_win);
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
    auto const window_size = 40;
    nc::initscr();
    nc::start_color();
    nc::noecho();
    nc::cbreak();
    nc::curs_set(0);
    nc::init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
    nc::init_pair(CYAN_ON_BLACK, COLOR_CYAN, COLOR_BLACK);
    nc::init_pair(MAGENTA_ON_BLUE, COLOR_MAGENTA, COLOR_BLUE);
    nc::init_pair(GREEN_ON_WHITE, COLOR_GREEN, COLOR_WHITE);
    nc::init_pair(RED_ON_BLACK, COLOR_RED, COLOR_BLACK);
    auto *main_win = nc::newwin(window_size, window_size, 0, 0);
    nc::leaveok(main_win, TRUE);
    nc::scrollok(main_win, TRUE);
    nc::wattr_set(main_win, A_NORMAL, WHITE_ON_BLACK, NULL);
    nc::keypad(main_win, true);

    auto seed = random_device()();
    auto room_id = sig(0);
    auto latest_visited_room = optional<sig>();
    auto next_free_room = sig(1);
    map<sig, map<plane_coord, sig>> room_network;
    while (true) {
        room_network[room_id] = {};
        random_gen rand(seed + static_cast<decltype(seed)>(room_id));
        auto const grid_size =
            sig(rand.get(window_size / 3, window_size / 2) + 5);
        auto *room_view =
            nc::subwin(main_win, int(grid_size), int(grid_size), 0, 0);
        auto *info_view =
            nc::subwin(main_win, 1, window_size, int(grid_size) + 1, 0);

        nc::wclear(main_win);
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
        auto exit_door =
            display_room(grid, room_network[room_id], main_win, room_view,
                         info_view, player_pos, "Room " + to_string(room_id));
        nc::delwin(room_view);
        nc::delwin(info_view);
        if (!exit_door)
            break;
        latest_visited_room = room_id;
        room_id = room_network.at(room_id).at(*exit_door);
    }

    nc::delwin(main_win);
    nc::endwin();
}
