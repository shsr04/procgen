#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <_main.hpp>
#include <chrono>
#include <sdl_wrap.hpp>
#include <thread>

#include "builder.hpp"
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
        static sig const damage = 0b1, catapult = 0b10, blaze = 0b100;
    };
    tile::idents tile; // needed?
    sig behavior;
    chrono::seconds activation_time;
    optional<tile::idents> employed_tiles;

    optional<sig> damage;
    chrono::seconds tmp_time = 0s; // used to count up
};

map<tile::idents, hazard> ALL_HAZARDS = {
    {tile::idents::dart_trap,
     {
         tile::idents::dart_trap,
         hazard::behavior_bits::catapult,
         5s,
         tile::idents::propelled_dart,
     }},
    {tile::idents::propelled_dart,
     {
         tile::idents::propelled_dart,
         hazard::behavior_bits::damage,
         -1s,
         {},
         5,
     }},
};

struct moving_object {
    tile::idents tile;
    plane_coord pos;
    pair<sig, sig> vel;
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
    auto moving_objects = vector<moving_object>();
    atomic<bool> hazard_ready = false;
    for (auto &c : grid[0]) {
        if (ALL_HAZARDS.find(grid[0][c]) != ALL_HAZARDS.end()) {
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
                /// When a hazard is ready, its tmp_time is 0 and
                /// hazard_ready is true.
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
        heartbeat_guard = scope_guard([&heartbeat, &heartbeat_active] {
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
            for (auto &[c, a] : active_hazards) {
                pair<sig, sig> vel = {rand.get(-1, 1), rand.get(-1, 1)};
                if (a.tmp_time > 0s)
                    continue;
                if ((vel.first | vel.second) == 0)
                    continue;
                if (a.behavior & hazard::behavior_bits::catapult) {
                    moving_objects.push_back({*a.employed_tiles, c, vel});
                }
            }
            hazard_ready = false;
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
            for (auto &[tile, pos, vel] : moving_objects) {
                grid[1][pos] = tile::idents::nil;
                pos.x() = pos.x() + vel.first;
                pos.y() = pos.y() + vel.second;
                grid[1][pos] = tile;
                if (pos == player.pos) {
                    grid[1][pos] = tile::idents::player;
                    auto damage = *ALL_HAZARDS.at(tile).damage;
                    player.life_points -= damage;
                }
            }
            moving_objects.erase(
                remove_if(begin(moving_objects), end(moving_objects),
                          [&grid](moving_object &m) {
                              if (m.pos.x().limit_reached() ||
                                  m.pos.y().limit_reached()) {
                                  grid[1][m.pos] = tile::idents::nil;
                                  return true;
                              }
                              return false;
                          }),
                end(moving_objects));
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
    specimen player = {.pos = {0, 0, 0, 0}, .life_points = 10};
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
            rand, grid, room_network[room_id], main_win, room_view, info_view,
            font, move(player), "Room " + to_string(room_id));
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
