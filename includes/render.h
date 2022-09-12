#ifndef RENDER_H_
#define RENDER_H_

#include <stdbool.h>
#include <vulkan/vulkan_core.h>
#include <SDL2/SDL.h>

#include "chair.h"

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

    /* Fragment shader code with an entry points */
    VkShaderModule frag;

    /* Pool from which command buffers are allocated from */
    VkCommandPool cmd_pool;

    /* Commands to be submitted to the device queue */
    VkCommandBuffer cmd_buffer;

    /* ??? */
    VkSemaphore image_available;
    /* ??? */
    VkSemaphore render_finished;
    /* ??? */
    VkFence in_flight_fence;
} RenderContext;

void vulkan_engine_create(RenderContext *context);
void vulkan_engine_destroy(RenderContext *context);
void vulkan_engine_render(RenderContext *context);

void sdl_renderer_create(RenderContext *context);
void sdl_renderer_destroy(RenderContext *context);

#endif // RENDER_H_
