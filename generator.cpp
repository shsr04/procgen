#include <_main.hpp>
#include <iterator>

vector<char> symbols = {'#', '.', '~', 'O'};
vector<double> p_symbols = {0.3, 0.5, 0.1, 0.1};

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

int main() {
    random_gen rand;
    auto const length = rand.get(20, 40) + 10;
    vector<vector<char>> grid(length, vector<char>());
    for (auto i_row : v::iota(0, length)) {
        for (auto i_col : v::iota(0, length)) {
            grid[i_row].push_back(*choose_random(rand, symbols, p_symbols));
        }
    }
    for (auto &row : grid) {
        if (row == grid.front() || row == grid.back())
            continue;
        for (auto &c : row) {
            if (c != '#')
                continue;
            for (auto pr : {0_s, grid.size() - 1}) {
                if (auto head =
                        r::find_if(grid[pr], [](auto &x) { return x != '#'; });
                    head != end(grid[pr])) {
                    swap(*head, c);
                }
            }
            if (row.front() != '#')
                swap(c, row.front());
            else if (row.back() != '#')
                swap(c, row.back());
            break;
        }
    }
    for (auto &row : grid) {
        for (auto &col : row)
            cout << col;
        cout << "\n";
    }
}
