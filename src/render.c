#include "render.h"

void sdl_renderer_create(RenderContext *ctx) {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayMode displayInfo;
    SDL_GetCurrentDisplayMode(0, &displayInfo);

    ctx->window = SDL_CreateWindow(
        "Deep Down Bad",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        displayInfo.w * 2 / 3,
        displayInfo.h * 2 / 3,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    ctx->renderer = SDL_CreateRenderer(
        ctx->window,
        -1,
        SDL_RENDERER_SOFTWARE
    );

    info(
        "created window with size: %ix%i",
        displayInfo.w * 2 / 3,
        displayInfo.h * 2 / 3
    );

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderPresent(ctx->renderer);

    info("SDL2 created");
}

void sdl_renderer_destroy(RenderContext *ctx) {
    SDL_DestroyRenderer(ctx->renderer);
    SDL_DestroyWindow(ctx->window);
    SDL_Quit();

    info("SDL2 destroyed");
}

// NOTE: !!!!!!!!!!!!!!!!
//
// the overlay order on objects is determined by who comes last in the indices
// array, therefore to overlay one object over the other only required having
// it be later in the indices array.
void vertices_data_create(RenderContext *ctx) {
    float x = -1.0;
    float y = -1.0;
    float x_inc = 2.0 / 16.0;
    float y_inc = 2.0 / 9.0;
    u32 grid_size = 13 * 9;
    u16 idx = 0;

    /* --------------------- assign vertices --------------------- */

    ctx->vertices_count = 4;
    ctx->vertices = vmalloc(ctx->vertices_count * sizeof(Vertex));

    ctx->vertices[0].pos[0] = -0.5;
    ctx->vertices[0].pos[1] = -0.5;
    ctx->vertices[0].tex[0] = 1.0;
    ctx->vertices[0].tex[1] = 0.0;

    ctx->vertices[1].pos[0] = 0.5;
    ctx->vertices[1].pos[1] = -0.5;
    ctx->vertices[1].tex[0] = 0.0;
    ctx->vertices[1].tex[1] = 0.0;

    ctx->vertices[2].pos[0] = 0.5;
    ctx->vertices[2].pos[1] = 0.5;
    ctx->vertices[2].tex[0] = 0.0;
    ctx->vertices[2].tex[1] = 1.0;

    ctx->vertices[3].pos[0] = -0.5;
    ctx->vertices[3].pos[1] = 0.5;
    ctx->vertices[3].tex[0] = 1.0;
    ctx->vertices[3].tex[1] = 1.0;

    //ctx->vertices_count = 4 * grid_size;

    //ctx->vertices = vmalloc(ctx->vertices_count * sizeof(Vertex));

    //// create a 13x9 grid made out of 0.25x0.25 sized square's.
    //for (y = 0; y < 9; y++) {
    //    for (x = 3; x < 16; x++) {
    //        ctx->vertices[idx + 0].pos[0] = x_inc * x - 1.0;
    //        ctx->vertices[idx + 0].pos[1] = y_inc * y - 1.0;
    //        ctx->vertices[idx + 0].tex[0] = 1.0;
    //        ctx->vertices[idx + 0].tex[1] = 0.0;

    //        ctx->vertices[idx + 1].pos[0] = x_inc * x - 1.0 + x_inc;
    //        ctx->vertices[idx + 1].pos[1] = y_inc * y - 1.0;
    //        ctx->vertices[idx + 0].tex[0] = 0.0;
    //        ctx->vertices[idx + 0].tex[1] = 0.0;

    //        ctx->vertices[idx + 2].pos[0] = x_inc * x - 1.0 + x_inc;
    //        ctx->vertices[idx + 2].pos[1] = y_inc * y - 1.0 + y_inc;
    //        ctx->vertices[idx + 0].tex[0] = 0.0;
    //        ctx->vertices[idx + 0].tex[1] = 1.0;

    //        ctx->vertices[idx + 3].pos[0] = x_inc * x - 1.0;
    //        ctx->vertices[idx + 3].pos[1] = y_inc * y - 1.0 + y_inc;
    //        ctx->vertices[idx + 0].tex[0] = 1.0;
    //        ctx->vertices[idx + 0].tex[1] = 1.0;

    //        idx += 4;
    //    }
    //}

    /* --------------------- assign indices--------------------- */
    ctx->indices_count = 6;
    ctx->indices = vmalloc(ctx->indices_count * sizeof(u16));

    ctx->indices[0]=  0;
    ctx->indices[1]=  1;
    ctx->indices[2]=  2;
    ctx->indices[3]=  2;
    ctx->indices[4]=  3;
    ctx->indices[5]=  0;
    //ctx->indices_count = 6 * grid_size;

    //ctx->indices = vmalloc(ctx->indices_count * sizeof(u16));

    //// for each square set the indices to the next 4 vertices
    ////
    //// the base indices are [0, 1, 2, 2, 3, 0] so each other square
    //// will use the same indices but with the offset of it's index
    //for (idx = 0; idx < grid_size; idx++) {
    //    u16 off_ind = idx * 4;
    //    u16 off_idx = idx * 6;

    //    ctx->indices[off_idx + 0] = off_ind + 0;
    //    ctx->indices[off_idx + 1] = off_ind + 1;
    //    ctx->indices[off_idx + 2] = off_ind + 2;
    //    ctx->indices[off_idx + 3] = off_ind + 2;
    //    ctx->indices[off_idx + 4] = off_ind + 3;
    //    ctx->indices[off_idx + 5] = off_ind + 0;
    //}
}
