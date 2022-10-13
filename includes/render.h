#ifndef RENDER_H_
#define RENDER_H_

#include <vulkan/vulkan_core.h>
#include <SDL2/SDL.h>

#include "chair.h"

#define MAX_FRAMES_LOADED 2

/* One of the most important goals of Vulkan when it was created, is that
 * multi-GPU can be done “manually”. This is done by creating a VkDevice for
 * each of the GPUs you want to use, and then it is possible to share data
 * between VkDevices. A candidate for this would be to create a VkDevice on
 * your main dedicated GPU for the actual graphics, but keep a VkDevice for the
 * integrated GPU to use to run some physics calculations or other data. */

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

    /* View's into the swapchain's images */
    VkImageView *views;

    /* Collection of memory attachments used by the render pass */
    VkFramebuffer *framebuffers;

    /* Number of images in the swapchain */
    u32 image_count;
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
    float pos[2];
    float col[3];
} Vertex;

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

    /* ??? */
    u32 queue_family_indices;

    /* ??? */
    VkQueue present_queue;

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

    /* Vertices of an object to be renderer */
    Vertex *vertices;

    /* Number of vertices to be renderer */
    u32 vertices_count;

    /* Memory on the GPU that holds the `vertices` */
    VkDeviceMemory vertex_memory;

    /* Reference to the memory in `vertex_memory` */
    VkBuffer vertex_buf;

    /* Offsets into `vertices` */
    u16 *indices;

    /* Number of vertices for the vertices */
    u32 indices_count;

    /* Memory on the GPU that holds the `indices` */
    VkDeviceMemory index_memory;

    /* Reference to the memory in `indices_memory` */
    VkBuffer index_buf;

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
} RenderContext;

void vk_engine_create(RenderContext *context);
void vk_engine_destroy(RenderContext *context);
void vk_engine_render(RenderContext *context);

void sdl_renderer_create(RenderContext *context);
void sdl_renderer_destroy(RenderContext *context);

#endif // RENDER_H_
