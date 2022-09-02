#ifndef RENDER_H_
#define RENDER_H_ 1

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

/* One of the most important goals of Vulkan when it was created, is that
 * multi-GPU can be done “manually”. This is done by creating a VkDevice for
 * each of the GPUs you want to use, and then it is possible to share data
 * between VkDevices. A candidate for this would be to create a VkDevice on
 * your main dedicated GPU for the actual graphics, but keep a VkDevice for the
 * integrated GPU to use to run some physics calculations or other data. */

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;

} UI;

typedef struct {
    /* */
    VkPresentModeKHR present_mode;

    /* Interface to send images to the screen.
     * List of images, accessible by the operating system for display */
    VkSwapchainKHR swapchain;


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

    /* Data required for operating the swap chain */
    SwapChainDescriptor swapchain;

    /* Images received from the swapchain */
    VkImage images[144];

} RenderContext;

void vulkan_engine_create(RenderContext *context, SDL_Window *window);
void vulkan_engine_render();
void vulkan_engine_destroy();

void sdl_renderer_create(UI *ui);
void sdl_renderer_destroy(UI *ui);

#endif // RENDER_H_
