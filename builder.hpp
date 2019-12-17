#pragma once
#include <_main.hpp>
#include "coord.hpp"
#include "grid.hpp"
#include "tile.hpp"

grid replace_coords(grid grid, vector<plane_coord> coords,
                    tile::idents symbol) {
    for (auto &&c : coords) {
        grid[c] = symbol;
    }
    return grid;
}

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

pair<layers, vector<plane_coord>> build_room(random_gen &rand, sig grid_size) {
    auto chest_coords = random_plane_coords(
        rand, static_cast<sig>(rand.get(0, 2)), grid_size - 2, 1);
    auto &&[door_coords, bottom_grid] =
        add_random_doorways(rand, random_grid(rand, grid_size, ALL_TILES),
                            static_cast<sig>(rand.get(2, 6)));
    return {layers{replace_coords(move(bottom_grid), chest_coords,
                                  tile::idents::chest),
                   empty_grid(grid_size),empty_grid(grid_size)},
            move(door_coords)};
}
