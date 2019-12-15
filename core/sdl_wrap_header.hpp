#pragma once
#include <SDL2/SDL_error.h>
#include <cstdio>
#include <string>

#define DEF_COPY(name, ident)                                                  \
    name(name const &) = ident;                                                \
    name &operator=(name const &) = ident;
#define DEF_MOVE(name, ident)                                                  \
    name(name &&) = ident;                                                     \
    name &operator=(name &&) = ident;
#define DEF_COPY_MOVE(name, ident) DEF_COPY(name, ident) DEF_MOVE(name, ident)

#define INSERT_IMPL(name) #name
#define INSERT(name) INSERT_IMPL(name)

inline void assert_true(bool expr, std::string msg, int line = 0) {
    if (!expr) {
        fprintf(stderr, "[%d] %s: %s\n", line, msg.c_str(), SDL_GetError());
        std::terminate();
    }
}
