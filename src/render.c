#include "render.h"

void sdl_renderer_create(RenderContext *ctx) {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayMode display_info;
    SDL_GetCurrentDisplayMode(0, &display_info);

    ctx->window = SDL_CreateWindow(
        "Deep Down Bad",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        display_info.w * 2 / 3,
        display_info.h * 2 / 3,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    ctx->renderer = SDL_CreateRenderer(
        ctx->window,
        -1,
        SDL_RENDERER_SOFTWARE
    );

    info(
        "created window with size: %ix%i",
        display_info.w * 2 / 3,
        display_info.h * 2 / 3
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

Object *object_alloc(RenderContext *ctx) {
    // initially allocate 4 objects
    if (ctx->object_alloc_count == 0) {
        ctx->objects = vmalloc(4 * sizeof(Object));
        ctx->object_alloc_count = 4;
        ctx->object_count = 1;

        return &ctx->objects[0];
    }

    ctx->object_count++;

    // if memory for a new object exists, use it
    if (ctx->object_count < ctx->object_alloc_count)
        return &ctx->objects[ctx->object_count - 1];

    // if there isn't enough space, allocate twice as much and copy over objects
    ctx->object_alloc_count *= 2;
    ctx->objects = vrealloc(ctx->objects, ctx->object_alloc_count);

    return &ctx->objects[ctx->object_count - 1];
}

// appends object to list of objects
//
// pos is an array of positions:
// [top-left, top-right, bottom-right, bottom-left]
//
// img_path is the texture to be overlayed on the object
bool object_create(RenderContext *ctx, float pos[4][2], const char *img_path) {
    Object *obj = object_alloc(ctx);

    /* --------------------- assign vertices --------------------- */
    obj->vertices_count = 4;
    obj->vertices = vmalloc(obj->vertices_count * sizeof(Vertex));

    obj->vertices[0].pos[0] = pos[0][0];
    obj->vertices[0].pos[1] = pos[0][1];
    obj->vertices[0].tex[0] = 0.0;
    obj->vertices[0].tex[1] = 0.0;

    obj->vertices[1].pos[0] = pos[1][0];
    obj->vertices[1].pos[1] = pos[1][1];
    obj->vertices[1].tex[0] = 1.0;
    obj->vertices[1].tex[1] = 0.0;

    obj->vertices[2].pos[0] = pos[2][0];
    obj->vertices[2].pos[1] = pos[2][1];
    obj->vertices[2].tex[0] = 1.0;
    obj->vertices[2].tex[1] = 1.0;

    obj->vertices[3].pos[0] = pos[3][0];
    obj->vertices[3].pos[1] = pos[3][1];
    obj->vertices[3].tex[0] = 0.0;
    obj->vertices[3].tex[1] = 1.0;

    /* --------------------- assign indices--------------------- */
    obj->indices_count = 6;
    obj->indices = vmalloc(obj->indices_count * sizeof(u16));

    obj->indices[0] = 0;
    obj->indices[1] = 1;
    obj->indices[2] = 2;
    obj->indices[3] = 2;
    obj->indices[4] = 3;
    obj->indices[5] = 0;

    if (!vk_vertices_copy(ctx, obj)) {
        error("failed to copy to GPU vertices buffer");
        return false;
    }

    if (!vk_indices_copy(ctx, obj)) {
        error("failed to copy to GPU indices buffer");
        return false;
    }

    if (!vk_image_create(ctx, &obj->texture, img_path)) {
        error("failed to create image");
        return false;
    }

    if (!vk_image_sampler_create(ctx, &obj->texture))
        panic("failed to create image sampler");

    if (!vk_descriptor_sets_create(ctx, &obj->texture)) {
        error("failed to create descriptor sets");
        return false;
    }

    return true;
};

// transform object position by x and y amount
void object_transform(Object *obj, float x, float y) {
    for (u32 idx = 0; idx < obj->vertices_count; idx++) {
        obj->vertices[idx].pos[0] += x;
        obj->vertices[idx].pos[1] += y;
    }
}

// destroys all objects at once
void objects_destroy(RenderContext *ctx) {
    for (u32 idx = 0; idx < ctx->object_count; idx++) {
        Object *obj = &ctx->objects[idx];

        // destroy vertices
        free(obj->vertices);
        vkFreeMemory(ctx->driver, obj->vertices_mem, NULL);
        vkDestroyBuffer(ctx->driver, obj->vertices_buf, NULL);

        // destroy indices
        free(obj->indices);
        vkFreeMemory(ctx->driver, obj->indices_mem, NULL);
        vkDestroyBuffer(ctx->driver, obj->indices_buf, NULL);

        // destroy texture
        vkDestroyImageView(ctx->driver, obj->texture.view, NULL);
        vkDestroyImage(ctx->driver, obj->texture.image, NULL);
        vkFreeMemory(ctx->driver, obj->texture.mem, NULL);
        vkDestroySampler(ctx->driver, obj->texture.sampler, NULL);
    }

    free(ctx->objects);
    info("game entities destroyed");
}
