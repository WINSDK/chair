#include "chair.h"
#include "render.h"
#include <SDL2/SDL.h>

void sdl_renderer_create(UI *ui) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    atexit(SDL_Quit);

    SDL_DisplayMode displayInfo;
    SDL_GetCurrentDisplayMode(0, &displayInfo);

    ui->window = SDL_CreateWindow(
        app_name,
        0,
        0,
        displayInfo.w / 2,
        displayInfo.h / 2,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    ui->renderer = SDL_CreateRenderer(
        ui->window, 
        -1, 
        SDL_RENDERER_SOFTWARE
    );

    info("created window with size: %ix%i", 
            displayInfo.w / 2, displayInfo.h / 2);

    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE); 
    SDL_RenderClear(ui->renderer);
    SDL_RenderPresent(ui->renderer);

    info("SDL2 created");
}

void sdl_renderer_destroy(UI *ui) {
    SDL_DestroyWindow(ui->window);
    SDL_Quit();

    info("SDL2 destroyed");
}
