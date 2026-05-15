#ifndef WINDOW_H
#define WINDOW_H

#include <SDL3/SDL.h>
#include "gb.h"

enum window_result {
    WINDOW_OK  = 0,
    WINDOW_ERR = 1,
};

enum window_options {
    WINDOW_DEFAULT = 0,
    WINDOW_SHOW_DEBUG = 1,
};

struct window_game {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    uint8_t initialized;
};

struct window_debug {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *map1;
    SDL_Texture  *map2;
    uint8_t initialized;
};

struct window {
    struct window_game  game;
    struct window_debug debug;
    struct gb *gb;
    int should_quit;
};

enum window_result window_init(struct window *window, struct gb *gb, enum window_options options);
void window_poll_events(struct window *window);
void window_render(struct window *window);
enum window_result window_close(struct window *window);

#endif