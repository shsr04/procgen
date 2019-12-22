#include <_main.hpp>
#include <sdl_wrap.hpp>

#include "builder.hpp"
#include "color.hpp"
#include "coord.hpp"
#include "grid.hpp"
#include "tile.hpp"

sig const FRAMES_PER_SECOND = 24;

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
            if (tiles.find(grid[c]) == tiles.end()) {
                font.renderToSurface("?", color_idents::WHITE_ON_BLACK, win,
                                     rect.x + int(c.x()), rect.y + int(c.y()));
            }
            auto tile = tiles.at(grid[c]);
            font.renderToSurface(
                string({*get_tile_symbol(grid, tiles, doors, c)}), tile.color,
                win, rect.x + int(c.x()), rect.y + int(c.y()));
        }
}

struct hazard {
    enum class idents {
        dart_trap,
    };
    struct behavior_bits {
        static sig const none = 0b1, sling = 0b10, dissipate = 0b100,
                         lob = 0b1000, blast = 1 << 4;
    };
    sig behavior;
    chrono::seconds activation_time;
    chrono::seconds tmp_time = 0s; // used to count up

    optional<sig> damage;
    optional<sig> energy;
    vector<tile::idents> employed_tiles;
};

map<tile::idents, hazard> ALL_HAZARDS = {
    {tile::idents::dart_trap,
     {
         .behavior = hazard::behavior_bits::sling,
         .activation_time = 3s,
         .employed_tiles = {tile::idents::propelled_dart},
     }},
    {tile::idents::bomb_trap,
     {
         .behavior = hazard::behavior_bits::lob,
         .activation_time = 4s,
         .employed_tiles = {tile::idents::propelled_bomb},
     }},
    {tile::idents::propelled_dart,
     {
         .behavior = hazard::behavior_bits::none,
         .activation_time = -1s,
         .damage = 5,
         .energy = 10,
         .employed_tiles = {},
     }},
    {tile::idents::propelled_bomb,
     {
         .behavior =
             hazard::behavior_bits::none | hazard::behavior_bits::dissipate,
         .activation_time = 3s,
         .damage = 3,
         .energy = 4,
         .employed_tiles = {tile::idents::blazing_fire},
     }},
    {tile::idents::blazing_fire,
     {
         .behavior = hazard::behavior_bits::blast,
         .activation_time = -1s,
         .damage = 20,
         .energy = 3,
         .employed_tiles = {tile::idents::burned_rubble_pile},
     }},
};

struct moving_object {
    tile::idents tile;
    plane_coord pos;
    pair<sig, sig> vel;
    sig energy;
};

struct specimen {
    struct status_bits {
        static sig const normal = 0b0, dead = 0b1;
        status_bits() = delete;
    };
    sig status = status_bits::normal;
    plane_coord pos;
    sig life_points;
};

tuple<layers, vector<moving_object>, map<plane_coord, hazard>, specimen>
apply_movement(layers grid, vector<moving_object> moving_objects,
               map<plane_coord, hazard> active_hazards, specimen player) {
    for (auto &[tile, pos, vel, energy] : moving_objects) {
        auto v_x = abs(vel.first), v_y = abs(vel.second);
        grid[2][pos] = tile::idents::nil;
        while ((v_x > 0 || v_y > 0) && energy > 0) {
            energy--;
            if (v_x > 0) {
                if (vel.first > 0)
                    ++pos.x();
                else if (vel.first < 0)
                    --pos.x();
                v_x--;
            }
            if (v_y > 0) {
                if (vel.second > 0)
                    ++pos.y();
                else if (vel.second < 0)
                    --pos.y();
                v_y--;
            }

            if (pos == player.pos) {
                auto damage = *ALL_HAZARDS.at(tile).damage;
                player.life_points -= damage;
                energy = 0;
            }
            if (!tile_satisfies_flags(grid[0], ALL_TILES, pos,
                                      tile::flag_bits::passable)) {
                if (ALL_HAZARDS.at(tile).behavior &
                    hazard::behavior_bits::blast) {
                    grid[0] =
                        replace_coords(move(grid[0]), {pos},
                                       ALL_HAZARDS.at(tile).employed_tiles[0]);
                    cout << "blasting "<<pos<<"\n";
                    // if a hazard is blasted, it is destroyed
                    if (active_hazards.count(pos))
                        active_hazards.erase(pos);
                }
                energy = 0;
            }
        }
        grid[2][pos] = tile;
        if (energy <= 0) {
            if (ALL_HAZARDS.count(tile)) {
                if (ALL_HAZARDS.at(tile).behavior &
                    hazard::behavior_bits::dissipate) {
                    /// produce a hazardous effect on dissipation
                    active_hazards[pos] = ALL_HAZARDS.at(tile);
                } else
                    grid[2][pos] = tile::idents::nil;
            }
        }
    }
    return {move(grid), move(moving_objects), move(active_hazards),
            move(player)};
}

vector<moving_object>
prune_stagnant_objects(vector<moving_object> moving_objects) {
    moving_objects.erase(remove_if(begin(moving_objects), end(moving_objects),
                                   [](moving_object &m) {
                                       if ((m.energy <= 0) ||
                                           m.pos.x().limit_reached() ||
                                           m.pos.y().limit_reached()) {
                                           return true;
                                       }
                                       return false;
                                   }),
                         end(moving_objects));
    return moving_objects;
}

tuple<layers, vector<moving_object>>
trigger_primed_hazards(map<plane_coord, hazard> const &active_hazards,
                       layers grid, vector<moving_object> moving_objects,
                       specimen const &player) {
    for (auto &[c, a] : active_hazards) {
        if (a.tmp_time > 0s)
            continue;
        if (a.behavior &
            (hazard::behavior_bits::sling | hazard::behavior_bits::lob)) {
            // pair<sig, sig> vel = {rand.get(-1, 1), rand.get(-1, 1)};
            auto v_x = sig(player.pos.x()) - sig(c.x()),
                 v_y = sig(player.pos.y()) - sig(c.y()),
                 v_max = max(abs(v_x), abs(v_y));
            pair<sig, sig> vel = {v_x / v_max, v_y / v_max};
            auto energy = *ALL_HAZARDS.at(a.employed_tiles[0]).energy;
            if ((vel.first | vel.second) == 0) // misfire
                continue;
            moving_objects.push_back({a.employed_tiles[0], c, vel, energy});
        }
        if (a.behavior & hazard::behavior_bits::dissipate) {
            grid[2][c] = tile::idents::nil;
            auto energy = *ALL_HAZARDS.at(a.employed_tiles[0]).energy;
            vector<pair<sig, sig>> vels = {{0, -2}, {1, -1}, {2, 0},  {1, 1},
                                           {0, 2},  {-1, 1}, {-2, 0}, {-1, -1}};
            for (auto &vel : vels)
                moving_objects.push_back({a.employed_tiles[0], c, vel, energy});
        }
    }
    return {move(grid), move(moving_objects)};
}

optional<specimen> display_room(random_gen rand, layers grid,
                                map<plane_coord, sig> const &out_doors,
                                Window const &main_win,
                                SDL_Rect const &room_view,
                                SDL_Rect const &info_view, Font const &font,
                                specimen player, string room_title) {
    grid[1][player.pos] = tile::idents::player;
    auto interaction_point = optional<plane_coord>();
    auto info_text = "--- " + room_title + "---";
    auto active_hazards = map<plane_coord, hazard>();
    atomic<bool> hazard_ready = false;
    auto moving_objects = vector<moving_object>();
    for (auto &c : grid[0]) {
        if (ALL_HAZARDS.count(grid[0][c])) {
            active_hazards[c] = ALL_HAZARDS.at(grid[0][c]);
            active_hazards[c].tmp_time =
                active_hazards[c].activation_time / rand.get(1, 4);
        }
    }
    thread heartbeat;
    atomic<bool> heartbeat_active = false;
    scope_guard heartbeat_guard;
    if (!active_hazards.empty())
        heartbeat = thread([&active_hazards, &hazard_ready, &heartbeat_active] {
            heartbeat_active = true;
            while (heartbeat_active) {
                /// hazard_ready is set when a hazard's tmp_time has become 0.
                /// If it is still set here, we wait.
                if (hazard_ready)
                    continue;
                this_thread::sleep_for(1s);
                for (auto &[c, a] : active_hazards) {
                    a.tmp_time += 1s;
                    if (a.tmp_time == a.activation_time)
                        hazard_ready = true, a.tmp_time = 0s;
                }
            }
        }),
        heartbeat_guard.assign([&heartbeat, &heartbeat_active] {
            heartbeat_active = false;
            if (heartbeat.joinable())
                heartbeat.join();
        });

    while (true) {
        main_win.clear({0, 0, 0});
        print_grid(grid, ALL_TILES, out_doors, main_win, room_view, font);
        font.renderToSurface(info_text, color_idents::WHITE_ON_BLACK, main_win,
                             info_view.x, info_view.y);
        font.renderToSurface("HP: " + to_string(player.life_points) +
                                 "    XP: 0",
                             color_idents::WHITE_ON_BLACK, main_win,
                             info_view.x, info_view.y + 1);
        main_win.updateWindow();
        if (player.life_points <= 0) {
            main_win.clear({75, 50, 50});
            font.renderToSurface("YOU ARE DEAD -- PRESS RETURN",
                                 color_idents::RED_ON_BLACK, main_win,
                                 room_view.x + 1, room_view.y + 10);
            player.status = specimen::status_bits::dead;
            main_win.updateWindow();
            return player;
        }
        SDL_Event event;
        SDL_WaitEventTimeout(&event, 1000 / FRAMES_PER_SECOND);
        auto prev = player.pos;
        if (event.type == SDL_KEYDOWN) {
            auto key = event.key.keysym.sym;
            if (key == SDLK_w)
                --player.pos.y();
            else if (key == SDLK_s)
                ++player.pos.y();
            else if (key == SDLK_a)
                --player.pos.x();
            else if (key == SDLK_d)
                ++player.pos.x();
            else if (key == SDLK_e && interaction_point) {
                auto effect =
                    interact_with(rand, grid[0], ALL_TILES, *interaction_point);
                info_text = *effect.message;
                continue;
            } else if (key == SDLK_q)
                return {};
            else if (key == SDLK_z)
                player.life_points--;
        }

        if (hazard_ready) {
            hazard_ready = false;
            auto &&[_grid, _objects] = trigger_primed_hazards(
                active_hazards, move(grid), move(moving_objects), player);
            grid = move(_grid);
            moving_objects = move(_objects);
        }

        if (player.pos != prev) {
            if (tile_satisfies_flags(grid[0], ALL_TILES, player.pos,
                                     tile::flag_bits::interactable))
                interaction_point = player.pos;
            else
                interaction_point = {};

            if (!tile_satisfies_flags(grid[0], ALL_TILES, player.pos,
                                      tile::flag_bits::passable)) {
                player.pos = prev;
                if (!interaction_point)
                    continue;
            }

            if (tile_satisfies_flags(grid[0], ALL_TILES, player.pos,
                                     tile::flag_bits::transporting)) {
                return player;
            }

            auto described_coord =
                interaction_point ? *interaction_point : player.pos;
            info_text =
                get_description(grid[0], ALL_TILES, out_doors, described_coord);

            grid[1][prev] = tile::idents::nil;
            grid[1][player.pos] = tile::idents::player;
        }

        if (!moving_objects.empty()) {
            auto &&[_grid, _objects, _hazards, _player] =
                apply_movement(move(grid), move(moving_objects),
                               move(active_hazards), move(player));
            grid = move(_grid);
            moving_objects = prune_stagnant_objects(move(_objects));
            active_hazards = move(_hazards);
            player = move(_player);
        }
    }
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
    specimen player = {.pos = {0, 0, 0, 0}, .life_points = 100};
    map<sig, map<plane_coord, sig>> room_network;
    while (true) {
        room_network[room_id] = {};
        random_gen rand(seed + static_cast<decltype(seed)>(room_id));
        auto const grid_size =
            sig(rand.get(window_size / 4, window_size / 3) + 5);
        /// ATTENTION: The rects should only be accessed via
        /// font.renderToSurface()! Otherwise the actual pixel size of the
        /// font has to be considered when drawing something "by hand". That
        /// means: a rect of size [w,h] corresponds to an area of [w*f,h*f]
        /// pixels where f is the main font size.
        auto room_view =
            SDL_Rect{.x = 0, .y = 0, .w = int(grid_size), .h = int(grid_size)};
        auto info_view =
            SDL_Rect{.x = 0, .y = room_view.h + 1, .w = window_size, .h = 2};

        main_win.updateWindow();
        auto &&[grid, doors] = build_room(rand, grid_size);
        player.pos = plane_coord(grid[0].size() / 2, grid[0].size() / 2,
                                 grid[0].size() - 1, 0);
        if (latest_visited_room) {
            room_network[room_id][doors.back()] = *latest_visited_room;
            grid[0][doors.back()] = tile::idents::sliding_door;
            player.pos = *find_adjoining_tile(grid[0], ALL_TILES, doors.back(),
                                              tile::idents::doorway_sigil);
            doors.pop_back();
        }
        for (auto &&c_door : move(doors)) {
            room_network.at(room_id)[c_door] = next_free_room;
            next_free_room++;
        }
        auto o_player = display_room(
            rand, move(grid), room_network[room_id], main_win, room_view,
            info_view, font, move(player), "Room " + to_string(room_id));
        if (!o_player)
            break;
        player = *o_player;
        if (player.status == specimen::status_bits::dead) {
            SDL_Event event;
            while (!SDL_PollEvent(&event) || event.type != SDL_KEYDOWN ||
                   event.key.keysym.sym != SDLK_RETURN)
                ;
            break;
        }
        latest_visited_room = room_id;
        room_id = room_network.at(room_id).at(player.pos);
    }
}
