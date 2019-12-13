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
};

#include "tile.hpp"

#include "coord.hpp"
#include "grid.hpp"

vector<pair<tile::idents, double>> room_tile_probs = {
    {tile::idents::stone_rubble_pile, 0.3},
    {tile::idents::stone_flooring, 0.3},
    {tile::idents::cracked_stone_flooring, 0.2},
    {tile::idents::decorated_stone_flooring, 0.1},
    {tile::idents::stone_pillar, 0.1},
};

template <class T>
optional<T> choose_random(random_gen &r, vector<pair<T, double>> const &v) {
    double limit = 0.;
    for (auto &[t, p] : v) {
        limit += p;
        if (r.get_real(0., 1.) < limit)
            return t;
    }
    return {};
}

grid random_grid(random_gen &rand, sig _size,
                 map<tile::idents, tile> const &tiles) {
    using ti = tile::idents;
    auto size = static_cast<size_t>(_size);
    vector<vector<ti>> grid(size, vector<ti>(0));
    grid[0] = grid.back() = vector<ti>(size, ti::wall);
    for (auto i_row : v::iota(1_s, size - 1)) {
        grid[i_row].push_back(ti::wall);
        for (auto i_col : v::iota(1_s, size - 1)) {
            grid[i_row].push_back(*choose_random(rand, room_tile_probs));
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
    vector<wall_coord> r;
    generate_n(back_inserter(r), count, [&]() {
        auto c = wall_coord(door_side, rand.get(min_u, max_u), max_u);
        while (find(begin(r), end(r), c) != end(r)) {
            c.u() = rand.get(min_u, max_u);
        }
        door_side = next_side(door_side);
        return c;
    });
    return r;
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
    auto tile = tiles.at(static_cast<tile::idents>(grid[coord]));
    return tile.flags & flags;
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
        return "Door to room " + to_string(out_doors.at(coord));
    if (auto o_door =
            find_adjoining_tile(grid, tiles, coord, tile::idents::doorway);
        ident == tile::idents::doorway_sigil && o_door)
        return get_description(grid, tiles, out_doors, *o_door);
    return desc;
}

void print_grid(layers const &layers, map<tile::idents, tile> const &tiles,
                nc::WINDOW *win) {
    for (auto &grid : layers)
        for (auto &c : grid) {
            if (grid[c] == tile::idents::nil)
                continue;
            auto tile = tiles.at(grid[c]);
            nc::wattr_set(win, tile.attr, tile.color, NULL);
            mvwaddch(win, c.y(), c.x(), tile.symbol);
        }
}

optional<plane_coord> display_room(layers grid,
                                   map<plane_coord, sig> const &out_doors,
                                   nc::WINDOW *main_win, nc::WINDOW *room_view,
                                   nc::WINDOW *info_view,
                                   plane_coord player_pos, string room_title) {
    auto interaction_point = optional<plane_coord>();
    grid[1][player_pos] = tile::idents::player;
    print_grid(grid, all_tiles, room_view);
    wprintw(info_view, ("--- " + room_title + "---").c_str());
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
        else if (c == 'e' && interaction_point)
            /*TODO: get item*/;
        else if (c == 'q')
            return {};
        else if (c == KEY_RESIZE)
            nc::wrefresh(main_win);

        if (player_pos == prev)
            continue;

        if (tile_satisfies_flags(grid[0], all_tiles, player_pos,
                                 tile::flag_bits::interactable))
            interaction_point = player_pos;
        else
            interaction_point = {};
        if (!tile_satisfies_flags(grid[0], all_tiles, player_pos,
                                  tile::flag_bits::passable)) {
            player_pos = prev;
            if (!interaction_point)
                continue;
        }
        if (tile_satisfies_flags(grid[0], all_tiles, player_pos,
                                 tile::flag_bits::transporting)) {
            return player_pos;
        }

        grid[1][prev] = tile::idents::nil;
        grid[1][player_pos] = tile::idents::player;

        nc::wclear(main_win);
        print_grid(grid, all_tiles, room_view);
        auto described_coord =
            interaction_point ? *interaction_point : player_pos;
        auto descr =
            get_description(grid[0], all_tiles, out_doors, described_coord);
        mvwprintw(info_view, 0, 0, descr.c_str());
        // nc::wmove(room_view, int(pos.y()), int(pos.x()));
        nc::wrefresh(main_win);
    }
}

pair<layers, vector<plane_coord>> build_room(random_gen &rand, sig grid_size) {
    auto chest_coords = random_plane_coords(
        rand, static_cast<sig>(rand.get(0, 2)), grid_size - 2, 1);
    auto &&[door_coords, bottom_grid] =
        add_random_doorways(rand, random_grid(rand, grid_size, all_tiles),
                            static_cast<sig>(rand.get(2, 6)));
    return {
        layers{replace_coords(move(bottom_grid), chest_coords, tile::idents::chest),
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
        random_gen rand(seed + static_cast<decltype(seed)>(room_id));
        auto const grid_size =
            sig(rand.get(window_size / 3, window_size / 2) + 5);
        auto *room_view =
            nc::subwin(main_win, int(grid_size), int(grid_size), 0, 0);
        auto *info_view =
            nc::subwin(main_win, 1, int(grid_size), int(grid_size) + 1, 0);

        nc::wclear(main_win);
        auto &&[grid, doors] = build_room(rand, grid_size);
        auto player_pos = plane_coord(grid[0].size() / 2, grid[0].size() / 2,
                                      grid[0].size() - 1, 0);
        if (latest_visited_room) {
            room_network[room_id][doors.back()] = *latest_visited_room;
            player_pos = *find_adjoining_tile(grid[0], all_tiles, doors.back(),
                                              tile::idents::doorway_sigil);
            doors.pop_back();
        }
        for (auto &c_door : doors) {
            room_network[room_id][c_door] = next_free_room;
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
