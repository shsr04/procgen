#include <_main.hpp>
#include <curses.h>
#include <iterator>

using grid = vector<vector<char>>;
using layers = vector<grid>;

vector<char> const symbols = {'"', '.', '~', 'O'};
vector<double> const p_symbols = {0.3, 0.5, 0.1, 0.1};
char const wall_symbol = '#';

enum class side : int { LEFT = 0, RIGHT = 1, UP = 2, DOWN = 3 };
side next_side(side p) {
    return static_cast<side>((static_cast<int>(p) + 1) % 4);
}

struct plane_coord {
    int x, y;
    bool operator==(plane_coord const &p) const { return x == p.x && y == p.y; }
};
ostream &operator<<(ostream &o, plane_coord p) {
    o << "(" << p.x << "," << p.y << ")";
    return o;
}
struct wall_coord {
    side s;
    int u;
    bool operator==(wall_coord const &p) const { return s == p.s && u == p.u; }
    plane_coord to_plane(int max_xy) const {
        switch (s) {
        case side::LEFT:
            return {0, u};
        case side::RIGHT:
            return {max_xy, u};
        case side::UP:
            return {u, 0};
        case side::DOWN:
            return {u, max_xy};
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
        if (r.get_real(0., 1.) < last + p[a])
            return v[a];
    }
    return {};
}

grid random_grid(random_gen &rand, int size, vector<char> const &symbols,
                 vector<double> const &p) {
    grid grid(size, vector<char>());
    grid[0] = grid.back() = vector<char>(size, wall_symbol);
    for (auto i_row : v::iota(1, size - 1)) {
        grid[i_row].push_back('#');
        for (auto i_col : v::iota(1, size - 1)) {
            grid[i_row].push_back(*choose_random(rand, symbols, p_symbols));
        }
        grid[i_row].push_back('#');
    }
    return grid;
}

grid empty_grid(int size) { return grid(size, grid::value_type(size)); }

vector<wall_coord> random_wall_coords(random_gen &rand, int count, int lim_u) {
    auto door_side = side::LEFT;
    vector<wall_coord> r;
    generate_n(back_inserter(r), count, [&rand, &lim_u, &door_side]() {
        auto r = wall_coord{door_side, rand.get(5, lim_u - 5)};
        door_side = next_side(door_side);
        return r;
    });
    return r;
}

vector<plane_coord> to_plane_coords(vector<wall_coord> p, int lim_xy) {
    vector<plane_coord> r;
    for (auto &&w : p) {
        // cout << w;
        r.push_back(w.to_plane(lim_xy - 1));
    }
    return r;
}

grid replace_coords(grid grid, vector<plane_coord> coords, char symbol) {
    for (auto &&[x, y] : coords) {
        grid[y][x] = symbol;
    }
    return grid;
}

void print_grid(layers const &_grid, nc::WINDOW *win) {
    using namespace nc;
    for (auto &grid : _grid)
        for (auto y : v::iota(0_s, grid.size()))
            for (auto x : v::iota(0_s, grid.size()))
                if (grid[y][x] != 0)
                    mvwaddch(win, y, x, grid[y][x]);

    wrefresh(win);
}

int main() {
    using nc::stdscr;
    random_gen rand;
    auto const size = rand.get(10, 20) + 5;
    auto *win = nc::initscr();
    nc::refresh();
    nc::keypad(win, true);
    nc::noecho();
    nc::cbreak();

    auto doors =
        to_plane_coords(random_wall_coords(rand, rand.get(2, 6), size), size);
    auto grid = layers{
        replace_coords(random_grid(rand, size, symbols, p_symbols), doors, ' '),
        empty_grid(size)};
    plane_coord pos = {.x = size / 2, .y = size / 2};
    print_grid(grid, win);
    auto const move_player_to = [&grid, &pos, &win](auto x, auto y) {
        grid[1][pos.y][pos.x] = 0;
        pos = {x, y};
        grid[1][pos.y][pos.x] = '*';
        print_grid(grid, win);
    };
    while (true) {
        auto c=nc::getch();
        if (c == 'w')
            move_player_to(pos.x, pos.y - 1);
        else if (c == 's')
            move_player_to(pos.x, pos.y + 1);
        else if (c == 'a')
            move_player_to(pos.x - 1, pos.y);
        else if (c == 'd')
            move_player_to(pos.x + 1, pos.y);

        nc::refresh();
    }
    nc::endwin();
}
