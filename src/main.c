#include "render.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void event_loop(RenderContext *ctx) {
    const u8 *board = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    bool vert_update_required = false;

    for (;;) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                return;
        }

        if (board[SDL_SCANCODE_C] && board[SDL_SCANCODE_LCTRL])
            break;

        if (board[SDL_SCANCODE_W]) {
            object_transform(&ctx->objects[1], 0.0, -0.004);
            vert_update_required = true;
        }

        if (board[SDL_SCANCODE_S]) {
            object_transform(&ctx->objects[1], 0.0, 0.004);
            vert_update_required = true;
        }

        if (board[SDL_SCANCODE_D]) {
            object_transform(&ctx->objects[1], 0.003, 0.0);
            vert_update_required = true;
        }

        if (board[SDL_SCANCODE_A]) {
            object_transform(&ctx->objects[1],  -0.003, 0.0);
            vert_update_required = true;
        }

        if (vert_update_required) {
            if (!vk_vertices_update(ctx, &ctx->objects[1]))
                warn("failed to copy vertices");

            vert_update_required = false;
        }

        vk_engine_render(ctx);
    }
}

int main(int argc, const char *argv[]) {
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

    RenderContext ctx;
    struct timespec t = now();

    sdl_renderer_create(&ctx);
    vk_engine_create(&ctx);

    t = time_elapsed(t);
    info("%lld.%.9ld seconds elapsed to initialize vulkan",
         (long long)t.tv_sec, t.tv_nsec);

    event_loop(&ctx);
    vk_engine_destroy(&ctx);
    sdl_renderer_destroy(&ctx);
    
    return 0;
}
