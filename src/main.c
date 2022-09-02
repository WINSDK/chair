#include "chair.h"
#include "render.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void event_loop(UI *ui, RenderContext *context) {
    bool quit = false;
    SDL_Event event;

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) return;
        }

        vulkan_engine_render();
    }

    return;
}

int main(int argc, char *argv[]) {
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

    UI ui;
    sdl_renderer_create(&ui);

    RenderContext context;
    vulkan_engine_create(&context, ui.window);

    event_loop(&ui, &context);

    vulkan_engine_destroy();
    sdl_renderer_destroy(&ui);
    
    return 0;
}
