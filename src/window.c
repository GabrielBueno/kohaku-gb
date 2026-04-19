#include "window.h"

#include <stddef.h>
#include <assert.h>
#include "constants.h"

#define WINDOW_SCALE_X 4
#define WINDOW_SCALE_Y 4

static enum window_result init_game_window(struct window_game *game);
static enum window_result init_debug_window(struct window_debug *debug, int show_debug);

void render_game_window(struct window_game *game);

static enum window_result close_game_window(struct window_game *game);
static enum window_result close_debug_window(struct window_debug *debug);

enum window_result window_init(struct window *window, struct gb *gb, enum window_options options) {
    assert(window != NULL);

    window->game.initialized  = 0;
    window->debug.initialized = 0;
    window->gb                = gb;
    window->should_quit       = 0;

    enum window_result result = WINDOW_OK;

    if ((result = init_game_window(&window->game)) != WINDOW_OK)
        return result;

    if ((result = init_debug_window(&window->debug, options & WINDOW_SHOW_DEBUG)) != WINDOW_OK)
        return result;

    return WINDOW_OK;
}

void window_poll_events(struct window *window) {
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_QUIT:
            window->should_quit = 1;
            break;
        }
    }
}

void window_render(struct window *window) {
    render_game_window(&window->game);
}

enum window_result window_close(struct window *window) {
    close_game_window(&window->game);
    close_debug_window(&window->debug);

    return WINDOW_OK;
}


static enum window_result init_game_window(struct window_game *game) {
    SDL_Window *window = SDL_CreateWindow("kohaku", GAMEBOY_LCD_WIDTH*WINDOW_SCALE_X, GAMEBOY_LCD_HEIGHT*WINDOW_SCALE_Y, 0);

    if (window == NULL)
        return WINDOW_ERR;

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    if (renderer == NULL)
        return WINDOW_ERR;

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GAMEBOY_LCD_WIDTH, GAMEBOY_LCD_HEIGHT);

    if (texture == NULL)
        return WINDOW_ERR;

    game->window      = window;
    game->renderer    = renderer;
    game->texture     = texture;
    game->initialized = 1;

    return WINDOW_OK;
}

static enum window_result init_debug_window(struct window_debug *debug, int show_debug) {
    return WINDOW_OK;
}

void render_game_window(struct window_game *game) {
    SDL_RenderClear(game->renderer);
    SDL_RenderTexture(game->renderer, game->texture, NULL, NULL);
    SDL_RenderPresent(game->renderer);
}

static enum window_result close_game_window(struct window_game *game) {
    if (!game->initialized)
        return WINDOW_OK;

    SDL_DestroyTexture(game->texture);
    SDL_DestroyRenderer(game->renderer);
    SDL_DestroyWindow(game->window);

    return WINDOW_OK;
}

static enum window_result close_debug_window(struct window_debug *debug) {
    if (!debug->initialized)
        return WINDOW_OK;

    SDL_DestroyTexture(debug->map1);
    SDL_DestroyTexture(debug->map2);
    SDL_DestroyRenderer(debug->renderer);
    SDL_DestroyWindow(debug->window);

    return WINDOW_OK;
}