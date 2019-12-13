#include <_main.hpp>
#include <curses.h>

using nat = unsigned long long; // C++ guarantees >=64 bits for ULL
using sig = signed long long;

enum color_ident : short {
    WHITE_ON_BLACK = 0,
    CYAN_ON_BLACK = 1,
    MAGENTA_ON_BLUE = 2,
    GREEN_ON_WHITE = 3,
};

#include "coord.hpp"
#include "tile.hpp"

vector<pair<tile::idents, double>> room_tile_probs = {
    {tile::idents::stone_rubble_pile, 0.3},
    {tile::idents::stone_flooring, 0.3},
    {tile::idents::cracked_stone_flooring, 0.2},
    {tile::idents::decorated_stone_flooring, 0.1},
    {tile::idents::stone_pillar, 0.1},
};
char const wall_symbol = all_tiles.at(tile::idents::wall).symbol,
           doorway_symbol = all_tiles.at(tile::idents::doorway).symbol,
           chest_symbol = all_tiles.at(tile::idents::chest).symbol,
           player_symbol = all_tiles.at(tile::idents::player).symbol;

class grid {
    vector<vector<char>> layers_;
    class row_ref {
        vector<char> &row_;

      public:
        template <class R>
        row_ref(R &row) : row_(const_cast<decltype(row_)>(row)) {}
        operator decltype(row_) &() { return row_; }
        auto &operator[](sig j) { return row_[static_cast<size_t>(j)]; }
        auto &operator[](sig j) const { return row_[static_cast<size_t>(j)]; }
    };
    class grid_iterator {
        plane_coord pos_;
        sig max_;

      public:
        grid_iterator(sig x, sig y, sig max_xy)
            : pos_(x, y, max_xy, 0), max_(max_xy) {}
        auto operator++() {
            if (pos_.x() == max_)
                pos_.x() = 0, ++pos_.y();
            else
                ++pos_.x();
            return *this;
        }
        auto &operator*() { return pos_; }
        auto operator!=(grid_iterator const &p) const { return pos_ != p.pos_; }
    };

  public:
    grid(decltype(layers_) layers) : layers_(layers) {}
    auto operator[](plane_coord const &p) const {
        return layers_[size_t(p.y())][size_t(p.x())];
    }
    auto &operator[](plane_coord const &p) {
        return layers_[size_t(p.y())][size_t(p.x())];
    }
    auto operator[](pair<sig, sig> p) const {
        return operator[](plane_coord(p.first, p.second, size() - 1, 0));
    }
    auto &operator[](pair<sig, sig> p) {
        return operator[](plane_coord(p.first, p.second, size() - 1, 0));
    }
    auto begin() { return grid_iterator(0, 0, sig(layers_.size()) - 1); };
    auto end() {
        return grid_iterator(sig(layers_.size()) - 1, sig(layers_.size()) - 1,
                             sig(layers_.size()) - 1);
    }
    sig size() const { return static_cast<sig>(layers_.size()); }
};
using layers = vector<grid>;

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
    auto size = static_cast<size_t>(_size);
    vector<vector<char>> grid(size, vector<char>());
    grid[0] = grid.back() = vector<char>(size, wall_symbol);
    for (auto i_row : v::iota(1_s, size - 1)) {
        grid[i_row].push_back(wall_symbol);
        for (auto i_col : v::iota(1_s, size - 1)) {
            grid[i_row].push_back(
                all_tiles.at(*choose_random(rand, room_tile_probs)).symbol);
        }
        grid[i_row].push_back(wall_symbol);
    }
    return grid;
}

grid empty_grid(sig size) {
    return {vector<vector<char>>(size_t(size), vector<char>(size_t(size)))};
}

vector<wall_coord> random_wall_coords(random_gen &rand, sig count, sig max_u,
                                      sig min_u) {
    auto door_side = side::LEFT;
    vector<wall_coord> r;
    generate_n(back_inserter(r), count, [&rand, &max_u, &min_u, &door_side]() {
        auto r = wall_coord{door_side, rand.get(min_u, max_u)};
        door_side = next_side(door_side);
        return r;
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
        // cout << w;
        r.push_back(w.to_plane(max_xy, min_xy));
    }
    return r;
}

grid replace_coords(grid grid, vector<plane_coord> coords, char symbol) {
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
    auto grid1 = replace_coords(move(grid), doors, doorway_symbol);
    auto grid2 =
        replace_coords(move(grid1), door_sigils,
                       all_tiles.at(tile::idents::doorway_sigil).symbol);
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
                       plane_coord const &coord) {
    auto ident = static_cast<tile::idents>(grid[coord]);
    auto desc = tiles.at(ident).description;
    if (tile_satisfies_flags(grid, tiles, coord, tile::flag_bits::interactable))
        return "(Press e to interact with " + desc + ")";
    if (ident == tile::idents::doorway)
        return "Door to ...";
    if (auto o_door =
            find_adjoining_tile(grid, tiles, coord, tile::idents::doorway);
        ident == tile::idents::doorway_sigil && o_door)
        return get_description(grid, tiles, *o_door);
    return desc;
}

void print_grid(layers const &layers, map<tile::idents, tile> const &tiles,
                nc::WINDOW *win) {
    using namespace nc;
    for (auto &grid : layers)
        for (auto y : v::iota(0, grid.size()))
            for (auto x : v::iota(0, grid.size())) {
                auto c = plane_coord(x, y, 0, grid.size());
                if (grid[c] == 0)
                    continue;
                auto tile = tiles.at(static_cast<tile::idents>(grid[c]));
                wattr_set(win, tile.attr, tile.color, NULL);
                mvwaddch(win, y, x, grid[c]);
            }
}

void build_room() {
    random_gen rand;
    auto const size = sig(rand.get(10, 20) + 5);
    nc::initscr();
    nc::start_color();
    nc::init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
    nc::init_pair(CYAN_ON_BLACK, COLOR_CYAN, COLOR_BLACK);
    nc::init_pair(MAGENTA_ON_BLUE, COLOR_MAGENTA, COLOR_BLUE);
    nc::init_pair(GREEN_ON_WHITE, COLOR_GREEN, COLOR_WHITE);
    auto *main_win = nc::newwin(int(size) + 2, int(size) + 2, 0, 0);
    nc::wattr_set(main_win, A_NORMAL, WHITE_ON_BLACK, NULL);
    auto *room_view = nc::subwin(main_win, int(size), int(size), 0, 0);
    auto *info_view = nc::subwin(main_win, 1, int(size), int(size) + 1, 0);
    nc::keypad(main_win, true);
    nc::noecho();
    nc::cbreak();
    nc::curs_set(0);

    auto c_chests = random_plane_coords(rand, static_cast<sig>(rand.get(0, 2)),
                                        size - 2, 1);

    auto &&[c_doors, bottom_grid] =
        add_random_doorways(rand, random_grid(rand, size, all_tiles),
                            static_cast<sig>(rand.get(2, 6)));
    auto grid =
        layers{replace_coords(move(bottom_grid), c_chests, chest_symbol),
               empty_grid(size)};

    auto pos = plane_coord(size / 2, size / 2, size - 1, 0);
    auto interaction_point = optional<plane_coord>();
    print_grid(grid, all_tiles, room_view);
    wprintw(info_view, "--- Welcome ---");
    nc::wrefresh(main_win);
    while (true) {
        auto c = nc::wgetch(main_win);
        auto prev = pos;
        if (c == 'w')
            --pos.y();
        else if (c == 's')
            ++pos.y();
        else if (c == 'a')
            --pos.x();
        else if (c == 'd')
            ++pos.x();
        else if (c == 'q')
            break;
        else if (c == KEY_RESIZE)
            nc::wrefresh(main_win);

        if (pos == prev)
            continue;

        if (tile_satisfies_flags(grid[0], all_tiles, pos,
                                 tile::flag_bits::interactable))
            interaction_point = pos;
        else
            interaction_point = {};
        if (!tile_satisfies_flags(grid[0], all_tiles, pos,
                                  tile::flag_bits::passable)) {
            pos = prev;
            if (!interaction_point)
                continue;
        }
        if (tile_satisfies_flags(grid[0], all_tiles, pos,
                                 tile::flag_bits::transporting)) {
            nc::wclear(main_win);
            print_grid(grid, all_tiles, room_view);
            nc::mvwprintw(info_view, 0, 0, "TELEPORTING...");
            nc::wrefresh(main_win);
            continue;
        }

        grid[1][prev] = 0;
        grid[1][pos] = player_symbol;

        nc::wclear(main_win);
        print_grid(grid, all_tiles, room_view);
        auto described_coord = interaction_point ? *interaction_point : pos;
        auto descr =get_description(grid[0], all_tiles, described_coord);
        mvwprintw(info_view, 0, 0, descr.c_str());
        // nc::wmove(room_view, int(pos.y()), int(pos.x()));
        nc::wrefresh(main_win);
    }
    nc::delwin(main_win);
    nc::endwin();
}

int main() { build_room(); }
