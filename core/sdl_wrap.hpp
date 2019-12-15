#pragma once
#include "sdl_wrap_header.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <array>
#include <string>

class Init {
  public:
    Init(Uint32 flags = SDL_INIT_EVERYTHING) { SDL_Init(flags); }
    ~Init() { SDL_Quit(); }
    DEF_COPY_MOVE(Init, delete);
};

class Window {
    SDL_Window *window_;
    SDL_Surface *surface_;

  public:
    const int width_, height_;

    Window(int width, int height, std::string title, Uint32 flags = 0)
        : window_(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED, width, height,
                                   SDL_WINDOW_SHOWN | flags)),
          surface_(SDL_GetWindowSurface(window_)), width_(width),
          height_(height) {}
    ~Window() { SDL_DestroyWindow(window_); }
    DEF_COPY_MOVE(Window, delete)

    operator SDL_Window *() const { return window_; }
    operator SDL_Surface *() const { return surface_; }

    void updateWindow() const { SDL_UpdateWindowSurface(window_); }

    void clear(SDL_Color color, SDL_Rect const *area = NULL) const {
        auto format = surface_->format;
        SDL_FillRect(surface_, area,
                     SDL_MapRGB(format, color.r, color.g, color.b));
    }
};

/**
 * SDL renderer, not to be used together with OpenGL rendering.
 */
class Renderer {
    SDL_Renderer *renderer_;

  public:
    Renderer(SDL_Window *window, Uint32 flags)
        : renderer_(SDL_CreateRenderer(window, -1, flags)) {}
    ~Renderer() { SDL_DestroyRenderer(renderer_); }
    DEF_COPY(Renderer, delete);
    DEF_MOVE(Renderer, default);

    operator SDL_Renderer *() { return renderer_; }

    void setColor(std::array<Uint8, 4> rgba) {
        SDL_SetRenderDrawColor(renderer_, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    void clear(std::array<Uint8, 4> rgba = {0, 0, 0, 0}) {
        setColor(rgba);
        SDL_RenderClear(renderer_);
    }

    void present() { SDL_RenderPresent(renderer_); }

    SDL_Texture *textureFrom(SDL_Surface *surface) {
        return SDL_CreateTextureFromSurface(renderer_, surface);
    }

    void copy(SDL_Texture *texture, SDL_Rect *srcArea = nullptr,
              SDL_Rect *destArea = nullptr) {
        SDL_RenderCopy(renderer_, texture, srcArea, destArea);
    }

    void fill(SDL_Rect *area = nullptr) { SDL_RenderFillRect(renderer_, area); }
    void draw(SDL_Rect *area = nullptr) { SDL_RenderDrawRect(renderer_, area); }
};

/**
 * ATTENTION: Renderer and Surface cannot be used together!
 */
class Surface {
    SDL_Surface *surface_;

  public:
    Surface(SDL_Window *window) : surface_(SDL_GetWindowSurface(window)) {}
    ~Surface() { SDL_FreeSurface(surface_); }
    DEF_COPY(Surface, delete)
    DEF_MOVE(Surface, default)

    operator SDL_Surface *() { return surface_; }
};

/**
 * Don't forget to init the SDL2_image library:
 *      assert_true((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0, "PNG init
 * failed");
 */
class Image {
    SDL_Surface *image_;

  public:
    Image(std::string path) : image_(IMG_Load(path.c_str())) {
        assert_true(image_, "PNG loading failed, did you forget IMG_Init?");
    }
    ~Image() { SDL_FreeSurface(image_); }
    DEF_COPY(Image, delete);
    DEF_MOVE(Image, default);

    operator SDL_Surface *() { return image_; }

    void convertTo(SDL_PixelFormat *format) {
        auto r = SDL_ConvertSurface(image_, format, 0);
        assert_true(r, "surface conversion failed");
        SDL_FreeSurface(image_);
        image_ = r;
    }
};

/**
 * Don't forget to init the SDL2_ttf library:
 *      assert_true((TTF_Init() == 0, "TTF init failed");
 */
class Font {
    TTF_Font *font_;

    std::pair<int, int> getDimensions(std::string const &text) const {
        int w, h;
        TTF_SizeText(font_, text.c_str(), &w, &h);
        return {w, h};
    }

  public:
    Font(std::string name, int height)
        : font_(TTF_OpenFont(name.c_str(), height)) {
        assert_true(font_ != NULL, "OpenFont error");
    }
    ~Font() { TTF_CloseFont(font_); }
    DEF_COPY(Font, delete)
    DEF_MOVE(Font, delete)

    auto height() const { return TTF_FontHeight(font_); }

    auto renderToSurface(std::string text,
                         std::pair<SDL_Color, SDL_Color> colorPair,
                         SDL_Surface *target, int x, int y) const {
        auto [w, h] = getDimensions(text);
        auto &[fgColor, bgColor] = colorPair;
        auto *fontSurface =
            TTF_RenderText_Blended(font_, text.c_str(), fgColor);
        SDL_Rect targetArea{x * w, y * h, w, h};
        SDL_FillRect(
            target, &targetArea,
            SDL_MapRGB(target->format, bgColor.r, bgColor.g, bgColor.b));
        SDL_BlitSurface(fontSurface, NULL, target, &targetArea);
        SDL_FreeSurface(fontSurface);
    }
};
