#ifndef RENDER_H_
#define RENDER_H_ 1

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include "chair.h"

/* One of the most important goals of Vulkan when it was created, is that
 * multi-GPU can be done “manually”. This is done by creating a VkDevice for
 * each of the GPUs you want to use, and then it is possible to share data
 * between VkDevices. A candidate for this would be to create a VkDevice on
 * your main dedicated GPU for the actual graphics, but keep a VkDevice for the
 * integrated GPU to use to run some physics calculations or other data. */

// TODO: move this to the `RenderContext`
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} UI;

typedef struct {
    /* Basic surface capabilities */
    VkSurfaceCapabilitiesKHR capabilities;

    /* Supported Pixel formats and color spaces */
    VkSurfaceFormatKHR* formats;

    /* Number of supported formats */
    u32 format_count;

    /* Interface to send images to the screen.
     * List of images, accessible by the operating system for display */
    VkSwapchainKHR data;
} SwapChainDescriptor;

typedef struct {
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
    VkQueue present_queue;

    /* Option for different types of vsync or none at all */
    VkPresentModeKHR present_mode;

    /* Details regarding a swapchain */
    SwapChainDescriptor swapchain;

    /* Abstraction of platform specific window interactions */
    VkSurfaceKHR surface;

    /* Images received from the swapchain */
    VkImage images[144];
} RenderContext;

void vulkan_engine_create(RenderContext *context, SDL_Window *window);
void vulkan_engine_destroy(RenderContext *context);
void vulkan_engine_render();

void sdl_renderer_create(UI *ui);
void sdl_renderer_destroy(UI *ui);

#endif // RENDER_H_
