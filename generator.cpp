#include <_main.hpp>
#include <curses.h>
#include <iterator>

using nat = unsigned long long; // C++ guarantees >=64 bits for ULL
using sig = signed long long;

enum color_ident : short {
    WHITE_ON_BLACK = 0,
    CYAN_ON_BLACK = 1,
    MAGENTA_ON_BLUE = 2,
    GREEN_ON_WHITE = 3,
};

struct tile {
    struct flag_bits {
        static sig const none = 0b0, passable = 0b1, interactable = 0b10,
                         transporting = 0b100;
        flag_bits() = delete;
    };
    enum class idents : char {
        wall = '#',
        doorway = ' ',
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
vector<pair<tile::idents, double>> room_tile_probs = {
    {tile::idents::stone_rubble_pile, 0.3},
    {tile::idents::stone_flooring, 0.3},
    {tile::idents::cracked_stone_flooring, 0.2},
    {tile::idents::decorated_stone_flooring, 0.1},
    {tile::idents::stone_pillar, 0.1},
    {tile::idents::wall, 0},
    {tile::idents::doorway, 0},
    {tile::idents::chest, 0}};
char const wall_symbol = all_tiles.at(tile::idents::wall).symbol,
           doorway_symbol = all_tiles.at(tile::idents::doorway).symbol,
           chest_symbol = all_tiles.at(tile::idents::chest).symbol,
           player_symbol = all_tiles.at(tile::idents::player).symbol;

enum class side : sig { LEFT = 0, RIGHT = 1, UP = 2, DOWN = 3 };
side next_side(side p) {
    return static_cast<side>((static_cast<sig>(p) + 1) % 4);
}

class limited_val {
    sig val_;
    sig min_, max_;

  public:
    limited_val(sig val, sig max, sig min) : val_(val), min_(min), max_(max) {}
    operator sig() const { return val_; }
    auto operator=(sig x) {
        val_ = x;
        return *this;
    }
    auto operator++() {
        if (val_ < max_)
            val_++;
        return val_;
    }
    auto operator--() {
        if (val_ > min_)
            val_--;
        return val_;
    }
};

class plane_coord {
    sig min_, max_;
    limited_val x_, y_;

  public:
    plane_coord(sig x, sig y, sig max_xy, sig min_xy)
        : min_(min_xy), max_(max_xy), x_(x, max_, min_), y_(y, max_, min_) {}
    auto &x() { return x_; }
    auto &x() const { return x_; }
    auto &y() { return y_; }
    auto &y() const { return y_; }
    bool operator==(plane_coord const &p) const {
        return x_ == p.x_ && y_ == p.y_;
    }
    bool operator!=(plane_coord const &p) const { return !(operator==(p)); }
};
ostream &operator<<(ostream &o, plane_coord p) {
    o << "(" << p.x() << "," << p.y() << ")";
    return o;
}
struct wall_coord {
    side s;
    sig u;
    bool operator==(wall_coord const &p) const { return s == p.s && u == p.u; }
    plane_coord to_plane(sig max_xy, sig min_xy) const {
        switch (s) {
        case side::LEFT:
            return {0, u, max_xy, min_xy};
        case side::RIGHT:
            return {max_xy, u, max_xy, min_xy};
        case side::UP:
            return {u, 0, max_xy, min_xy};
        case side::DOWN:
            return {u, max_xy, max_xy, min_xy};
        }
    }
};
ostream &operator<<(ostream &o, wall_coord p) {
    char c = 0;
    switch (p.s) {
    case side::LEFT:
        c = 'L';
        break;
    case side::RIGHT:
        c = 'R';
        break;
    case side::UP:
        c = 'U';
        break;
    case side::DOWN:
        c = 'D';
        break;
    }
    o << "(" << c << "," << p.u << ")";
    return o;
}

class grid {
    vector<vector<char>> v_;
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
    grid(decltype(v_) v) : v_(v) {}
    grid(sig size) : v_(size_t(size), decltype(v_)::value_type(size_t(size))) {}
    auto operator[](plane_coord const &p) const {
        return v_[size_t(p.y())][size_t(p.x())];
    }
    auto &operator[](plane_coord const &p) {
        return v_[size_t(p.y())][size_t(p.x())];
    }
    auto begin() { return grid_iterator(0, 0, sig(v_.size()) - 1); };
    auto end() {
        return grid_iterator(sig(v_.size()) - 1, sig(v_.size()) - 1,
                             sig(v_.size()) - 1);
    }
    sig size() const { return static_cast<sig>(v_.size()); }
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

grid empty_grid(sig size) { return grid(size); }

vector<wall_coord> random_wall_coords(random_gen &rand, sig count, sig max_u) {
    auto door_side = side::LEFT;
    vector<wall_coord> r;
    generate_n(back_inserter(r), count, [&rand, &max_u, &door_side]() {
        auto r = wall_coord{door_side, rand.get(sig(5), max_u - 5)};
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

bool tile_satisfies_flags(grid const &grid, plane_coord const &coord,
                          map<tile::idents, tile> const &tiles, sig flags) {
    auto tile = tiles.at(static_cast<tile::idents>(grid[coord]));
    return tile.flags & flags;
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
    auto *win = nc::subwin(main_win, int(size), int(size), 0, 0);
    auto *info = nc::subwin(main_win, 1, int(size), int(size) + 1, 0);
    nc::keypad(main_win, true);
    nc::noecho();
    nc::cbreak();
    nc::curs_set(2);

    auto doors = to_plane_coords(
        random_wall_coords(rand, static_cast<sig>(rand.get(2, 6)), size),
        size - 1, 0);
    auto chests = random_plane_coords(rand, static_cast<sig>(rand.get(0, 2)),
                                      size - 1, 1);
    auto grid =
        layers{replace_coords(replace_coords(random_grid(rand, size, all_tiles),
                                             doors, doorway_symbol),
                              chests, chest_symbol),
               empty_grid(size)};

    auto pos = plane_coord(size / 2, size / 2, size - 1, 0);
    auto interaction_point = optional<plane_coord>();
    for (auto &c : grid[0]) {
        cout << c << " ";
    }
    print_grid(grid, all_tiles, win);
    wprintw(info, "--- Welcome ---");
    nc::wrefresh(main_win);
    while (true) {
        auto c = nc::wgetch(win);
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

        if (tile_satisfies_flags(grid[0], pos, all_tiles,
                                 tile::flag_bits::interactable))
            interaction_point = pos;
        else
            interaction_point = {};
        if (!tile_satisfies_flags(grid[0], pos, all_tiles,
                                  tile::flag_bits::passable)) {
            pos = prev;
            if (!interaction_point)
                continue;
        }

        grid[1][prev] = 0;
        grid[1][pos] = player_symbol;

        nc::wclear(main_win);
        print_grid(grid, all_tiles, win);
        auto descr =
            all_tiles
                .at(static_cast<tile::idents>(
                    grid[0][interaction_point ? *interaction_point : pos]))
                .description;
        if (!interaction_point)
            mvwprintw(info, 0, 0, descr.c_str());
        else
            mvwprintw(
                info, 0, 0,
                string("(Press 'e' to interact with " + descr + ")").c_str());
        nc::wrefresh(main_win);
        nc::wmove(main_win, int(pos.y()), int(pos.x()));
    }
    nc::delwin(main_win);
    nc::endwin();
}

int main() { build_room(); }
