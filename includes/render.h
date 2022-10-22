#ifndef RENDER_H_
#define RENDER_H_

#include <vulkan/vulkan_core.h>
#include <SDL2/SDL.h>

#include "utils.h"

#define MAX_FRAMES_LOADED 2

/* One of the most important goals of Vulkan when it was created, is that
 * multi-GPU can be done “manually”. This is done by creating a VkDevice for
 * each of the GPUs you want to use, and then it is possible to share data
 * between VkDevices. A candidate for this would be to create a VkDevice on
 * your main dedicated GPU for the actual graphics, but keep a VkDevice for the
 * integrated GPU to use to run some physics calculations or other data. */

/* Data being sent to the vertex shader
 *
 * pos is in location 0
 * tex is in location 1 */
typedef struct Vertex {
    f32 pos[2];
    f32 tex[2];
} __attribute__ ((aligned (8))) Vertex;

typedef struct {
    /* Interface to send images to the screen.
     * List of images, accessible by the operating system for display */
    VkSwapchainKHR data;

    /* Basic surface capabilities */
    VkSurfaceCapabilitiesKHR capabilities;

    /* Supported Pixel formats and color spaces */
    VkSurfaceFormatKHR* formats;

    /* Number of supported formats */
    u32 format_count;

    /* Images received from the swapchain */
    VkImage* images;

    /* Number of images in the swapchain */
    u32 image_count;

    /* View's into the swapchain's images */
    VkImageView *views;

    /* Collection of memory attachments used by the render pass */
    VkFramebuffer *framebuffers;
} SwapChainDescriptor;

typedef struct {
    /* Layer properties */
    VkLayerProperties* data;

    /* Enabled validation layers for debugging */
    const char** layers;

    /* Count of validation layers for debugging */
    u32 layer_count;
} ValidationLayers;

typedef struct {
    /* ??? */
    VkSemaphore images_available;

    /* ??? */
    VkSemaphore renders_finished;

    /* Lock indicating whether or not the next frame can be drawn */
    VkFence renderers_busy;
} Synchronization;

typedef struct {
    /* Memory on the GPU that holds the `texture` */
    VkDeviceMemory mem;

    /* Reference to the memory in `texture_memory` */
    VkImage image;

    /* Additional metadata and resources references required by shaders */
    VkImageView view;

    /* Descriptor bindings for every frame */
    VkDescriptorSet desc_sets[MAX_FRAMES_LOADED];

    /* Method of reading images, applying filters and other transformations */
    VkSampler sampler;
} Texture;

/* Objects could be stored in a linked list.
 * However as every member of Object and it's members are just pointers
 * I feel like the cost of copying over the struct on appends isn't too bad */
typedef struct {
    /* Unique texture identifier */
    u32 ident;

    /* Vertices of the object to be renderer */
    Vertex *vertices;

    /* Number of vertices to be renderer */
    u32 vertices_count;

    /* Memory on the GPU that holds the `vertices` */
    VkDeviceMemory vertices_mem;

    /* Reference to the memory in `vertex_memory` */
    VkBuffer vertices_buf;

    /* Everything related to the object's texture */
    Texture texture;
} Object;

typedef struct {
    /* SDL application state */
    SDL_Window *window;

    /* SDL render related state */
    SDL_Renderer *renderer;

    /* Vulkan API Context */
    VkInstance instance;

    /* Handle to debug messenger */
    VkDebugUtilsMessengerEXT messenger;

    /* GPU being used in the system */
    VkPhysicalDevice device;

    /* GPU driver on the GPU hardware */
    VkDevice driver;

    /* Interface for which to send command buffers to the GPU */
    VkQueue queue;

    /* Index of the queue family that supports graphics commands */
    u32 queue_family;

    /* Option for different types of vsync or none at all */
    VkPresentModeKHR present_mode;

    /* Information related to validation layers */
    ValidationLayers validation;

    /* Information related to the swapchain */
    SwapChainDescriptor swapchain;

    /* Abstraction of platform specific window interactions */
    VkSurfaceKHR surface;

    /* Format we've choosen to use for the surface */
    VkSurfaceFormatKHR surface_format;

    /* Resolution of the swap chain images */
    VkExtent2D dimensions;

    /* Complete description of the resources the pipeline can access */
    VkPipelineLayout pipeline_layout;

    /* Information of the required sequence of operations for doing a draw call */
    VkPipeline pipeline;

    /* Collection of attachments, subpasses, and dependencies between the subpasses */
    VkRenderPass render_pass;

    /* State's in the pipeline that can be mutated without recreating the pipeline again */
    VkDynamicState *dynamic_states;

    /* Number of dynamic states used */
    u32 dynamic_state_count;

    /* Where in the framebuffer to render to */
    VkViewport viewport;

    /* Region of the viewport to actually display */
    VkRect2D scissor;

    /* Vertex shader code with an entry points */
    VkShaderModule vert;

    /* Fragment shader code with an entry points */
    VkShaderModule frag;

    /* Pool from which command buffers are allocated from */
    VkCommandPool cmd_pool;

    /* Commands to be submitted to the device queue */
    VkCommandBuffer cmd_bufs[MAX_FRAMES_LOADED];

    /* Synchronization objects required by `vk_engine_render`. */
    Synchronization sync[MAX_FRAMES_LOADED];

    /* Index of the current frame being renderer */
    u32 frame;

    /* Details related to allocating memory on the GPU */
    VkPhysicalDeviceMemoryProperties mem_prop;

    /* Details related to the GPU */
    VkPhysicalDeviceProperties dev_prop;

    /* Description on how a VkDescriptorSet should be created */
    VkDescriptorSetLayout desc_set_layout;

    /* Pool from which descriptor sets are allocated */
    VkDescriptorPool desc_pool;

    /* Entities in the game to be rendered */
    Object *objects;

    /* Number of entities to be rendered */
    u32 object_count;

    /* Number of entities allocated in `objects` */
    u32 object_alloc_count;

    /* Memory on the GPU that holds a `tile_staging_buf` */
    VkDeviceMemory tile_staging_mem;

    /* Intermediate buffer for writing 16x16 tile's vertices */
    VkBuffer tile_staging_buf;

    /* Memory address of `tile_staging_buf` */
    void *tile_gpu_mem;

    /* Memory on the GPU that holds a `player_staging_buf` */
    VkDeviceMemory player_staging_mem;

    /* Intermediate buffer for writing player's vertices */
    VkBuffer player_staging_buf;

    /* Memory address of `player_staging_buf` */
    void *player_gpu_mem;

    /* Offsets into `vertices` */
    u16 *indices;

    /* Indices into `vertices` on what vertices to draw */
    u32 indices_count;

    /* Memory on the GPU that holds the `indices` */
    VkDeviceMemory indices_mem;

    /* Reference to the memory in `indices_memory` */
    VkBuffer indices_buf;

    /* Memory on the GPU that holds the `indices_staging_buf` */
    VkDeviceMemory indices_staging_mem;

    /* Intermediate buffer for writing to the indices on the GPU */
    VkBuffer indices_staging_buf;

    /* Memory address of `indices_staging_buf` */
    void *indices_gpu_mem;
} RenderContext;

typedef struct {
    /* Whether or not the menu is open */
    bool menu_open;

    /* Whether or not the window is in fullscreen mode */
    bool fullscreen;

    /* Indicator that the game should be exited ASAP */
    bool quit_game;

    /* Indicator that the vertices have changed */
    bool update_vertices;

    /* Movement generated since last position update */
    f32 dx, dy;
} Game;

typedef enum {
    OBJECT_PLAYER,
    OBJECT_TILE
} ObjectType;

void vk_engine_create(RenderContext *ctx);
void vk_engine_destroy(RenderContext *ctx);
void vk_engine_render(RenderContext *ctx);

bool vk_descriptor_sets_create(RenderContext *ctx, Texture *tex);
bool vk_image_sampler_create(RenderContext *ctx, Texture *tex);

bool vk_image_create(RenderContext *ctx, Texture *tex, const char *path);
bool vk_image_from_surface(RenderContext *ctx, Texture *tex, SDL_Surface *img);

bool vk_swapchain_recreate(RenderContext *ctx);

bool vk_vertices_create(RenderContext *ctx, Object *obj, ObjectType type);
bool vk_vertices_update(RenderContext *ctx, Object *obj, ObjectType type);

bool vk_indices_create(RenderContext *ctx);
bool vk_indices_update(RenderContext *ctx);

void sdl_renderer_create(RenderContext *ctx);
void sdl_renderer_destroy(RenderContext *ctx);

bool level_map_load(RenderContext *ctx,
                    const char *level_path,
                    const char *tileset_path);

SDL_Surface *sdl_load_image(const char *path);

bool object_create(RenderContext *ctx, f32 pos[4][2], const char *img_path);
void object_transform(Object *obj, f32 x, f32 y);
void objects_destroy(RenderContext *ctx);

Object *object_find(RenderContext *ctx, u32 ident);
bool object_find_destroy(RenderContext *ctx, u32 ident);

#endif // RENDER_H_
