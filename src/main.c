#include "chair.h"
#include "render.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

bool event_loop(SDL_Renderer *renderer, SDL_Window *window) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    bool quit = false;
    SDL_Event event;

    RenderContext context;
    if (!vulkan_engine_create(&context, window)) quit = true;

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) quit = true;
        }

        if (!vulkan_engine_render()) quit = true;
    }

    return true;
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    atexit(SDL_Quit);

    if (argc > 1) {
        if (strcmp(argv[1], "--error") == 0) {
            set_log_level(LOG_ERROR);
        } else if (strcmp(argv[1], "--warn") == 0) {
            set_log_level(LOG_WARN);
        } else if (strcmp(argv[1], "--trace") == 0) {
            set_log_level(LOG_TRACE);
        } else if (strcmp(argv[1], "--info") == 0) {
            set_log_level(LOG_INFO);
        }
    }

    SDL_DisplayMode displayInfo;
    SDL_GetCurrentDisplayMode(0, &displayInfo);

    SDL_Window *window = SDL_CreateWindow(
        app_name,
        SDL_WINDOW_VULKAN,
        SDL_WINDOW_VULKAN,
        displayInfo.w / 2,
        displayInfo.h / 2,
        0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_SOFTWARE
    );

    info("created window with size: %ix%i", 
            displayInfo.w / 2, displayInfo.h / 2);

    if (!event_loop(renderer, window))
        panic("event loop failed");
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
