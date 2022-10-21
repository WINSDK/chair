#include "render.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static f32 BG_COORDS[4][2] = {
    { -1.0, -1.0 },
    {  1.0, -1.0 },
    {  1.0,  1.0 },
    { -1.0,  1.0 }
};

static f32 FULLSCREEN_REGION[2][2] = {
    { 0.217578, 0.266250 },
    { 0.780078, 0.365625 }
};

static f32 QUIT_REGION[2][2] = {
    { 0.392969, 0.590000 },
    { 0.599609, 0.691875 }
};

static f32 ROOM_REGION[2][2] = {
    { -1.0, -1.0 },
    {  1.0,  1.0 }
};

static const u8 *KEYBOARD;

/* Returns whether coords are within a square region.
 *
 * region[0] is top-left
 * region[1] is bottom-right 
 *
 */
bool coord_in_region(f32 coords[2], f32 region[2][2]) {
    return coords[0] >= region[0][0] && coords[0] <= region[1][0] &&
           coords[1] >= region[0][1] && coords[1] <= region[1][1];
}

void handler_mouse(RenderContext *ctx, Game *state, i32 x, i32 y) {
    f32 pos[2];

    // normalize cursor coordinates
    pos[0] = (f32)x / (f32)ctx->dimensions.width;
    pos[1] = (f32)y / (f32)ctx->dimensions.height;

    trace("x: %f, y: %f", pos[0], pos[1]);

    if (state->menu_open && coord_in_region(pos, FULLSCREEN_REGION)) {
        SDL_SetWindowFullscreen(
            ctx->window,
            state->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP
        );

        vk_swapchain_recreate(ctx);
        objects_recreate(ctx);
        state->fullscreen = !state->fullscreen;
    }

    if (state->menu_open && coord_in_region(pos, QUIT_REGION))
        state->quit_game = true;
}

void handler_keyboard(RenderContext *ctx, Game *game, f64 dt) {
    f64 speed = 5.0;
    f32 vertical = (f32)(KEYBOARD[SDL_SCANCODE_S] - KEYBOARD[SDL_SCANCODE_W]);
    f32 horizontal = (f32)(KEYBOARD[SDL_SCANCODE_D] - KEYBOARD[SDL_SCANCODE_A]);

    if (vertical != 0.0 && horizontal != 0.0) {
        // sqrt(1.0**2 + 1.0**2) / 2
        speed *= 0.707;
    }

    game->dy += vertical * (f32)(16.0 * speed * dt);
    game->dx += horizontal * (f32)(9.0 * speed * dt);
}

void handler_event(RenderContext *ctx, Game *game) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            game->quit_game = true;

        if (event.type == SDL_MOUSEBUTTONDOWN)
            handler_mouse(ctx, game, event.button.x, event.button.y);

        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {

            if (game->menu_open) {
                object_find_destroy(ctx, "./assets/escape_menu.bmp");
            } else {
                object_create(ctx, BG_COORDS, "./assets/escape_menu.bmp");
            }

            game->menu_open = !game->menu_open;
        }
    }
}

/* Detects whether an object will collide with the environment and returns
 * whether this collision will occur */
void resolve_collisions(Object *obj, Game *game) {
    for (u32 idx = 0; idx < obj->vertices_count; idx++) {
        f32 pos[2];

        pos[0] = obj->vertices[idx].pos[0] + game->dx;
        pos[1] = obj->vertices[idx].pos[1] + game->dy;

        // check if x coordinate would collide
        if (pos[0] < ROOM_REGION[0][0] || pos[0] > ROOM_REGION[1][0]) {
            game->dx = 0.0;
        }

        // check if y coordinate would collide
        if (pos[1] < ROOM_REGION[0][1] || pos[1] > ROOM_REGION[1][1]) {
            game->dy = 0.0;
        }
    }
}

void render(RenderContext *ctx, Game *game, f64 dt) {
    Object *player = object_find(ctx, "./assets/guy.bmp");

    handler_keyboard(ctx, game, dt);

    if (game->dx != 0.0 || game->dy != 0.0) {
        resolve_collisions(player, game);
        object_transform(player, game->dx, game->dy);
        vk_vertices_update(ctx, player, OBJECT_PLAYER);

        game->dx = 0.0;
        game->dy = 0.0;
    }
}

void event_loop(RenderContext *ctx) {
    struct timespec time = {};
    Game game = {};

    for (;;) {
        handler_event(ctx, &game);
        render(ctx, &game, time_elapsed(&time));
        vk_engine_render(ctx);

        if (game.quit_game)
            break;

        now(&time);
    }
}

i32 main(i32 argc, const char *argv[]) {
    RenderContext ctx;
    struct timespec time;

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

    now(&time);

    sdl_renderer_create(&ctx);
    KEYBOARD = SDL_GetKeyboardState(NULL);

    vk_engine_create(&ctx);

    info("%lf seconds elapsed to initialize vulkan", time_elapsed(&time));

    event_loop(&ctx);
    vk_engine_destroy(&ctx);
    sdl_renderer_destroy(&ctx);
    
    return 0;
}
