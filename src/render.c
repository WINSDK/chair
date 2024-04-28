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
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );

    if (!ctx->window)
        panic("failed to create window: %s", SDL_GetError());


    info(
        "created window with size: %ix%i",
        display_info.w * 2 / 3,
        display_info.h * 2 / 3
    );

    info("SDL2 created");
}

void sdl_renderer_destroy(RenderContext *ctx) {
    SDL_DestroyWindow(ctx->window);
    SDL_Quit();

    info("SDL2 destroyed");
}

SDL_Surface *sdl_load_image(const char *path) {
    SDL_Surface *img, *conv_img;

    if (!(img = SDL_LoadBMP(path))) {
        return NULL;
    }

    /* ------- convert surface to `VK_FORMAT_B8G8R8A8_SRGB` ------- */
    conv_img = SDL_ConvertSurfaceFormat(
        img,
        SDL_PIXELFORMAT_BGRA32,
        0
    );

    if (!conv_img) {
        error("failed to convert image to 'VK_FORMAT_B8G8R8A8_SRGB'");
        return NULL;
    }

    SDL_FreeSurface(img);

    return conv_img;
}

SDL_Surface *tileset_sprite_load(RenderContext *ctx,
                                 SDL_Surface *tileset,
                                 SDL_Rect *sprite_region) {

    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(
        0,
        sprite_region->w,
        sprite_region->h,
        16,
        SDL_PIXELFORMAT_BGRA32
    );

    if (SDL_BlitSurface(tileset, sprite_region, dst, NULL) == -1) {
        error("failed to copy tileset surface region");
        return NULL;
    }

    return dst;
}

Object *object_alloc(RenderContext *ctx) {
    /* Initially allocate 32 * 18 * 2 + 2 objects.
     *
     * Enough for a two room's, a player and an exit menu. */

    if (ctx->object_alloc_count == 0) {
        ctx->object_alloc_count = 32 * 18 * 2 + 2;
        ctx->object_count = 1;

        ctx->objects = vmalloc(ctx->object_alloc_count * sizeof(Object));

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

/* Loads a sprite from a level map into an object */
bool object_from_tile(RenderContext *ctx,
                      u32 x, u32 y,
                      SDL_Surface *tile,
                      u32 ident) {

    f32 block_w = 1.0 / 16.0;
    f32 block_h = 1.0 / 9.0;
    Object *obj = object_alloc(ctx);

    obj->ident = ident;

    /* --------------------- assign vertices --------------------- */
    obj->vertices_count = 4;
    obj->vertices = vmalloc(obj->vertices_count * sizeof(Vertex));

    obj->vertices[0].pos[0] = -1.0 + x * block_w;
    obj->vertices[0].pos[1] = -1.0 + y * block_h;
    obj->vertices[0].tex[0] = 0.0;
    obj->vertices[0].tex[1] = 0.0;

    obj->vertices[1].pos[0] = -1.0 + (x + 1) * block_w;
    obj->vertices[1].pos[1] = -1.0 + y * block_h;
    obj->vertices[1].tex[0] = 1.0;
    obj->vertices[1].tex[1] = 0.0;

    obj->vertices[2].pos[0] = -1.0 + (x + 1) * block_w;
    obj->vertices[2].pos[1] = -1.0 + (y + 1) * block_h;
    obj->vertices[2].tex[0] = 1.0;
    obj->vertices[2].tex[1] = 1.0;

    obj->vertices[3].pos[0] = -1.0 + x * block_w;
    obj->vertices[3].pos[1] = -1.0 + (y + 1) * block_h;
    obj->vertices[3].tex[0] = 0.0;
    obj->vertices[3].tex[1] = 1.0;

    if (!vk_vertices_create(ctx, obj, OBJECT_TILE)) {
        error("failed to create GPU vertices buffer");
        return false;
    }

    if (!vk_image_from_surface(ctx, &obj->texture, tile)) {
        error("failed to create image");
        return false;
    }

    if (!vk_image_sampler_create(ctx, &obj->texture)) {
        error("failed to create image sampler");
        return false;
    }

    if (!vk_descriptor_sets_create(ctx, &obj->texture)) {
        error("failed to create descriptor sets");
        return false;
    }

    return true;
};

/* Appends object to list of objects
 *
 * pos is an array of positions:
 * [top-left, top-right, bottom-right, bottom-left]
 *
 * img_path is the texture to be overlayed on the object */
bool object_create(RenderContext *ctx, f32 pos[4][2], const char *img_path) {
    Object *obj = object_alloc(ctx);

    obj->ident = HASH(img_path);

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

    if (!vk_vertices_create(ctx, obj, OBJECT_PLAYER)) {
        error("failed to create GPU vertices buffer");
        return false;
    }

    if (!vk_image_create(ctx, &obj->texture, img_path)) {
        error("failed to create image");
        return false;
    }

    if (!vk_image_sampler_create(ctx, &obj->texture)) {
        error("failed to create image sampler");
        return false;
    }

    if (!vk_descriptor_sets_create(ctx, &obj->texture)) {
        error("failed to create descriptor sets");
        return false;
    }

    return true;
};

void object_destroy(RenderContext *ctx, Object *obj) {
    // destroy vertices
    free(obj->vertices);
    vkFreeMemory(ctx->driver, obj->vertices_mem, NULL);
    vkDestroyBuffer(ctx->driver, obj->vertices_buf, NULL);

    // destroy texture
    vkDestroyImageView(ctx->driver, obj->texture.view, NULL);
    vkDestroyImage(ctx->driver, obj->texture.image, NULL);
    vkFreeMemory(ctx->driver, obj->texture.mem, NULL);
    vkDestroySampler(ctx->driver, obj->texture.sampler, NULL);
}

/* Destroys all objects at once. */
void objects_destroy(RenderContext *ctx) {
    for (u32 idx = 0; idx < ctx->object_count; idx++)
        object_destroy(ctx, &ctx->objects[idx]);

    free(ctx->objects);
    info("game entities destroyed");
}

Object *object_find(RenderContext *ctx, u32 ident) {
    Object *obj = NULL;

    for (u32 idx = 0; idx < ctx->object_count; idx++)
        if (ctx->objects[idx].ident == ident)
            obj = &ctx->objects[idx];

    return obj;
}

/* Tries to destroy an object and returns whether or not it succeeded.
 *
 * Copies the last elements of the array into the deleted spot. */
bool object_find_destroy(RenderContext *ctx, u32 ident) {
    Object *obj = object_find(ctx, ident);

    if (obj == NULL || ctx->object_count == 0)
        return false;

    // wait for driver to finish queued work then destroy object resources
    vkDeviceWaitIdle(ctx->driver);
    object_destroy(ctx, obj);

    memcpy(obj, &ctx->objects[ctx->object_count - 1], sizeof(Object));
    ctx->object_count--;

    return true;
}

/* Transform object position by x and y amount. */
void object_transform(Object *obj, f32 x, f32 y) {
    for (u32 idx = 0; idx < obj->vertices_count; idx++) {
        obj->vertices[idx].pos[0] += x;
        obj->vertices[idx].pos[1] += y;
    }
}

/* Returns surfaces for all the different tileset tiles */
SDL_Surface **level_tileset_load(RenderContext *ctx, const char *path) {
    SDL_Surface **surfaces = vmalloc(25 * 25 * sizeof(SDL_Surface));
    SDL_Surface *tileset = sdl_load_image("./assets/tileset.bmp");

    for (u32 x = 0; x < 25; x++) {
        for (u32 y = 0; y < 25; y++) {

            SDL_Rect region = {
                y * 16, x * 16,
                16, 16
            };

            surfaces[x * 25 + y] = tileset_sprite_load(
                ctx,
                tileset,
                &region
            );

            if (!surfaces[x * 25 + y]) {
                SDL_FreeSurface(tileset);
                error("failed to load a sprite from tileset: '%s'", path);
                return NULL;
            }
        }
    }

    SDL_FreeSurface(tileset);

    return surfaces;
}

void level_tileset_destroy(SDL_Surface **tileset) {
    for (u32 idx = 0; idx < 25 * 25; idx++) {
        SDL_FreeSurface(tileset[idx]);
    }

    free(tileset);
}

/* Creates objects for each of the squares listed in a level map file.
 *
 * Takes a path to an CSV file of 16 rows and 9 columns. */
bool level_map_load(RenderContext *ctx,
                    const char *level_path,
                    const char *tileset_path) {

    SDL_Surface **tileset;
    FILE *level;
    u32 x = 0, y = 0;

    if (!(tileset = level_tileset_load(ctx, tileset_path))) {
        error("failed to load tileset: '%s'", level_path);
        return false;
    }

    if (!(level = fopen(level_path, "r"))) {
        error("failed to read level map: '%s'", level_path);
        level_tileset_destroy(tileset);
        return false;
    }

    // read csv
    for (;;) {
        u32 idx;
        int args = fscanf(level, "%d", &idx);
        int delim = fgetc(level);

        if (args != 1 && delim != ',' && delim != '\n' && delim != '\0') {
            level_tileset_destroy(tileset);
            fclose(level);
            return true;
        }

        if (idx == -1) {
            x++;

            if (delim == '\n') {
                y++;
                x = 0;
            }

            continue;
        }

        trace("idx: %d, x: %d, y: %d", idx, x, y);

        if (!object_from_tile(ctx, x, y, tileset[idx], idx)) {
            level_tileset_destroy(tileset);
            fclose(level);
            error("failed to create object from tile");
            return false;
        }

        if (delim == '\n') {
            y++;
            x = 0;
            continue;
        }

        if (delim == EOF)
            break;

        x++;
    }

    level_tileset_destroy(tileset);
    fclose(level);

    return true;
}
