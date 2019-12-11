#include <_main.hpp>
#include <curses.h>
#include <iterator>

using grid = vector<vector<char>>;
using layers = vector<grid>;

enum color_ident : short {
    WHITE_ON_BLACK = 0,
    CYAN_ON_BLACK = 1,
    MAGENTA_ON_BLUE = 2,
};

struct tile {
    enum class flag_bits : char {
        none = 0b0,
        passable = 0b1,
        special = 0b10,
    };
    char symbol;
    double prob;
    flag_bits flags;
    string description = "";
    color_ident color = WHITE_ON_BLACK;
    nc::chtype attr = A_NORMAL;
};

tile const wall_tile = {'#', 0, tile::flag_bits::none};
tile const doorway_tile = {' ', 0, tile::flag_bits::passable};
tile const chest_tile = {'?', 0, tile::flag_bits::special, "a mysterious chest",
                         MAGENTA_ON_BLUE};
vector<tile> const room_tiles = {
    {
        '"',
        0.3,
        tile::flag_bits::passable,
        "small pile of rubble",
    },
    {
        '.',
        0.3,
        tile::flag_bits::passable,
        "stone floor",
    },
    {
        ',',
        0.2,
        tile::flag_bits::passable,
        "cracked stone floor",
    },
    {
        '~',
        0.1,
        tile::flag_bits::passable,
        "decorated stone floor",
    },
    {
        'O',
        0.1,
        tile::flag_bits::none,
        "stone pillar",
        CYAN_ON_BLACK,
    },
    wall_tile,
    doorway_tile,
    chest_tile,
};

string tile_description(char sym, vector<tile> const &tiles) {
    return r::find_if(tiles, [sym](auto &x) { return sym == x.symbol; })
        ->description;
}

enum class side : int { LEFT = 0, RIGHT = 1, UP = 2, DOWN = 3 };
side next_side(side p) {
    return static_cast<side>((static_cast<int>(p) + 1) % 4);
}

class limited_val {
    int val_;
    int min_, max_;

  public:
    limited_val(int val, int max, int min) : val_(val), min_(min), max_(max) {}
    operator int() const { return val_; }
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
    int min_, max_;
    limited_val x_, y_;

  public:
    plane_coord(int x, int y, int max_xy, int min_xy)
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
    int u;
    bool operator==(wall_coord const &p) const { return s == p.s && u == p.u; }
    plane_coord to_plane(int max_xy, int min_xy) const {
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

template <class T>
optional<T> choose_random(random_gen &r, vector<T> const &v,
                          vector<double> const &p) {
    double last = 0.;
    for (auto a : v::iota(0_s, v.size())) {
        if (r.get_real(0, 1) < last + p[a])
            return v[a];
        last += p[a];
    }
    return {};
}

grid random_grid(random_gen &rand, int size, vector<tile> const &tiles) {
    grid grid(size, vector<char>());
    grid[0] = grid.back() = vector<char>(size, wall_tile.symbol);
    for (auto i_row : v::iota(1, size - 1)) {
        grid[i_row].push_back('#');
        auto symbols = tiles | v::transform([](auto &x) { return x.symbol; });
        auto p_symbols = tiles | v::transform([](auto &x) { return x.prob; });
        for (auto i_col : v::iota(1, size - 1)) {
            grid[i_row].push_back(*choose_random(
                rand, vector<char>{r::begin(symbols), r::end(symbols)},
                {r::begin(p_symbols), r::end(p_symbols)}));
        }
        grid[i_row].push_back('#');
    }
    return grid;
}

grid empty_grid(int size) { return grid(size, grid::value_type(size)); }

vector<wall_coord> random_wall_coords(random_gen &rand, int count, int max_u) {
    auto door_side = side::LEFT;
    vector<wall_coord> r;
    generate_n(back_inserter(r), count, [&rand, &max_u, &door_side]() {
        auto r = wall_coord{door_side, rand.get(5, max_u - 5)};
        door_side = next_side(door_side);
        return r;
    });
    return r;
}

vector<plane_coord> random_plane_coords(random_gen &rand, int count, int max_xy,
                                        int min_xy) {
    vector<plane_coord> r;
    generate_n(back_inserter(r), count, [&rand, &max_xy, &min_xy]() {
        return plane_coord{rand.get(min_xy, max_xy), rand.get(min_xy, max_xy),
                           max_xy, min_xy};
    });
    return r;
}

vector<plane_coord> to_plane_coords(vector<wall_coord> p, int max_xy,
                                    int min_xy) {
    vector<plane_coord> r;
    for (auto &&w : p) {
        // cout << w;
        r.push_back(w.to_plane(max_xy, min_xy));
    }
    return r;
}

grid replace_coords(grid grid, vector<plane_coord> coords, char symbol) {
    for (auto &&c : coords) {
        grid[c.y()][c.x()] = symbol;
    }
    return grid;
}

bool tile_satisfies_flags(grid const &grid, plane_coord const &coord,
                          vector<tile> const &tiles, tile::flag_bits flags) {
    auto sym = grid[coord.y()][coord.x()];
    auto tile = r::find_if(tiles, [sym](auto &x) { return x.symbol == sym; });
    return static_cast<char>(tile->flags) & static_cast<char>(flags);
}

void print_grid(layers const &_grid, vector<tile> const &tiles,
                nc::WINDOW *win) {
    using namespace nc;
    for (auto &grid : _grid)
        for (auto y : v::iota(0_s, grid.size()))
            for (auto x : v::iota(0_s, grid.size())) {
                if (grid[y][x] == 0)
                    continue;
                if (auto tile = r::find_if(tiles,
                                           [sym = grid[y][x]](auto &x) {
                                               return sym == x.symbol;
                                           });
                    tile != tiles.end())
                    wattr_set(win, tile->attr, tile->color, NULL);
                mvwaddch(win, y, x, grid[y][x]);
            }
}

void build_room() {
    random_gen rand;
    auto const size = rand.get(10, 20) + 5;
    nc::initscr();
    nc::start_color();
    nc::init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
    nc::init_pair(CYAN_ON_BLACK, COLOR_CYAN, COLOR_BLACK);
    nc::init_pair(MAGENTA_ON_BLUE, COLOR_MAGENTA, COLOR_BLUE);
    auto *win = nc::newwin(size, size, 0, 0);
    auto *info = nc::newwin(2, size, size + 1, 0);
    nc::wattr_set(info, A_NORMAL, WHITE_ON_BLACK, NULL);
    nc::wrefresh(win);
    nc::wrefresh(info);
    nc::keypad(win, true);
    nc::noecho();
    nc::cbreak();

    auto doors = to_plane_coords(random_wall_coords(rand, rand.get(2, 6), size),
                                 size - 1, 0);
    auto chests = random_plane_coords(rand, rand.get(0, 2), size - 1, 1);
    auto grid = layers{
        replace_coords(replace_coords(random_grid(rand, size, room_tiles),
                                      doors, doorway_tile.symbol),
                       chests, chest_tile.symbol),
        empty_grid(size)};

    auto pos = plane_coord(size / 2, size / 2, size - 1, 0);
    print_grid(grid, room_tiles, win);
    wprintw(info, "TEST ---");

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

        if (pos != prev) {
            if (tile_satisfies_flags(grid[0], pos, room_tiles,
                                     tile::flag_bits::special)) {
                nc::wclear(info);
                mvwprintw(info, 0, 0, "(Press 'e' to open chest)");
                nc::wrefresh(info);
            }
            if (!tile_satisfies_flags(grid[0], pos, room_tiles,
                                      tile::flag_bits::passable)) {
                pos = prev;
                continue;
            }
            grid[1][prev.y()][prev.x()] = 0;
            grid[1][pos.y()][pos.x()] = '*';
        }
        nc::wclear(win);
        nc::wclear(info);
        print_grid(grid, room_tiles, win);
        nc::wrefresh(win);
        mvwprintw(
            info, 0, 0,
            tile_description(grid[0][pos.y()][pos.x()], room_tiles).c_str());
        nc::wrefresh(info);
        nc::wmove(win, pos.y(), pos.x());
    }
    nc::delwin(win);
    nc::endwin();
}

int main() { build_room(); }
