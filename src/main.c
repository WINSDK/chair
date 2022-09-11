#include "render.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void event_loop(RenderContext *context) {
    bool quit = false;
    SDL_Event event;

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) return;
        }

        vulkan_engine_render(context);
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

    RenderContext context = {};

    struct timespec t = now();
    sdl_renderer_create(&context);
    vulkan_engine_create(&context);

    t = time_elapsed(t);
    info("%lld.%.9ld seconds elapsed to initialize vulkan",
         (long long)t.tv_sec, t.tv_nsec);

    event_loop(&context);
    vulkan_engine_destroy(&context);
    sdl_renderer_destroy(&context);
    
    return 0;
}
