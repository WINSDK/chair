#include "render.h"
#include <SDL2/SDL.h>
#include <signal.h>

void sdl_renderer_create(RenderContext *context) {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_DisplayMode displayInfo;
    SDL_GetCurrentDisplayMode(0, &displayInfo);

    context->window = SDL_CreateWindow(
        "SDL2",
        0,
        0,
        displayInfo.w / 2,
        displayInfo.h / 2,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    context->renderer = SDL_CreateRenderer(
        context->window,
        -1,
        SDL_RENDERER_SOFTWARE
    );

    info("created window with size: %ix%i", 
            displayInfo.w / 2, displayInfo.h / 2);

        SDL_SetRenderDrawColor(context->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(context->renderer);
    SDL_RenderPresent(context->renderer);

    info("SDL2 created");
}

void sdl_renderer_destroy(RenderContext *context) {
    SDL_DestroyRenderer(context->renderer);
    SDL_DestroyWindow(context->window);
    SDL_Quit();

    info("SDL2 destroyed");
}
