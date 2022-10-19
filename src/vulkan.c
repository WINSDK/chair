#include "utils.h"
#include "render.h"

#include <SDL2/SDL_vulkan.h>
#include <stddef.h>
#include <stdlib.h>

const char **get_required_extensions(SDL_Window *window, u32 *count) {
    const char **extensions;

    if (!SDL_Vulkan_GetInstanceExtensions(window, count, NULL)) {
        error("failed to retrieve all required extensions: '%s'",
              SDL_GetError());

        return NULL;
    }

    extensions = vmalloc((*count + 2) * sizeof(char *));

    if (!SDL_Vulkan_GetInstanceExtensions(window, count, extensions)) {
        error("failed to retrieve all required extensions: '%s'",
              SDL_GetError());
        return NULL;
    }

    // MacOS requires the `VK_KHR_PORTABILITY_subset` extension
#ifdef __APPLE__
    extensions[(*count)++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif
    extensions[(*count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    trace_array(extensions, *count, "extensions enabled: ");

    return extensions;
}

const char **get_optional_extensions(u32 *count) {
    VkExtensionProperties *extensions;
    const char **extensions_names;

    vkEnumerateInstanceExtensionProperties(NULL, count, NULL);

    extensions = vmalloc(*count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, count, extensions);

    extensions_names = vmalloc(*count * sizeof(char *));
    for (u32 idx = 0; idx < *count; idx++)
        extensions_names[idx] = extensions[idx].extensionName;

    trace_array(extensions_names, *count, "available extensions: ");
    free(extensions);

    return extensions_names;
}

u32 vk_find_memory_type(RenderContext *ctx, VkMemoryRequirements reqs,
                        VkMemoryPropertyFlags flags) {

    for (u32 idx = 0; idx < ctx->mem_prop.memoryTypeCount; idx++) {
        if ((reqs.memoryTypeBits & (1 << idx)) &&
            ctx->mem_prop.memoryTypes[idx].propertyFlags & flags) {
            return idx;
        }
    }

    warn("failed to find any compatible memory type");
    return 0;
}

bool matches_device_requirements(VkPhysicalDevice device) {
    u32 count, idx;
    VkPhysicalDeviceFeatures features;
    VkExtensionProperties *extensions;
    bool required_extensions_found = false;

    vkGetPhysicalDeviceFeatures(device, &features);

    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL))
        return false;

    extensions = vmalloc(count * sizeof(VkExtensionProperties));
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions))
        return false;

    for (idx = 0; idx < count; idx++) {
        const char *name = extensions[idx].extensionName;

        if (strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            required_extensions_found = true;
    }

    if (!required_extensions_found) {
        free(extensions);
        return false;
    }

    if (get_log_level() == LOG_TRACE) {
        const char **extension_names = vmalloc(count * sizeof(char *));

        for (idx = 0; idx < count; idx++)
            extension_names[idx] = extensions[idx].extensionName;

        trace_array(extension_names, count, "available device extensions: ");
        free(extension_names);
    }

    free(extensions);
    return features.geometryShader && features.samplerAnisotropy;
}

// Sets correct present mode on success. In the case of failing to find the
// preferred present mode, the present mode is left untouched.
bool try_preferred_present_mode(RenderContext *ctx,
                                VkPresentModeKHR *present_mode) {
    u32 count, idx;
    VkResult present_support_result;
    VkPresentModeKHR *present_modes;

    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        ctx->device, ctx->surface, &count, NULL);

    present_modes = vmalloc(count * sizeof(VkPresentModeKHR *));
    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        ctx->device, ctx->surface, &count, present_modes);

    if (present_support_result || count == 0) {
        free(present_modes);
        return false;
    }

    if (get_log_level() == LOG_TRACE) {
        const char **names = vmalloc(count * sizeof(char *));

        for (idx = 0; idx < count; idx++) {
            switch (present_modes[idx]) {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                names[idx] = "VK_PRESENT_MODE_IMMEDIATE_KHR";
                break;
            case VK_PRESENT_MODE_MAILBOX_KHR:
                names[idx] = "VK_PRESENT_MODE_MAILBOX_KHR";
                break;
            case VK_PRESENT_MODE_FIFO_KHR:
                names[idx] = "VK_PRESENT_MODE_FIFO_KHR";
                break;
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                names[idx] = "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
                break;
            case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
                names[idx] = "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
                break;
            case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
                names[idx] = "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
                break;
            case VK_PRESENT_MODE_MAX_ENUM_KHR:
                names[idx] = "VK_PRESENT_MODE_MAX_ENUM_KHR";
                break;
            }
        }

        trace_array(names, count, "supported present modes: ");
        free(names);
    }

    // do nothing if the `preferred_present_mode` is supported
    for (idx = 0; idx < count; idx++) {
        if (present_modes[idx] == *present_mode) {
            free(present_modes);
            return true;
        }
    }

    free(present_modes);
    return false;
}

// Sets correct swapchain format on success. In the case of failing to find the
// preferred swapchain, the format is left untouched.
bool try_preferred_swapchain_format(RenderContext *ctx) {
    u32 idx;
    VkSurfaceFormatKHR surface_format;

    for (idx = 0; idx < ctx->swapchain.format_count; idx++) {
        surface_format = ctx->swapchain.formats[idx];

        if (surface_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            ctx->surface_format = surface_format;
            return true;
        }
    }

    return false;
}

/// Find a queue family that supports graphics.
bool find_queue_families(RenderContext *ctx) {
    u32 count;
    VkQueueFamilyProperties *families;

    vkGetPhysicalDeviceQueueFamilyProperties(ctx->device, &count, NULL);
    families = vmalloc(count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->device, &count, families);

    for (u32 idx = 0; idx < count; idx++) {
        if (families[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            ctx->queue_family_indices = idx;

            free(families);
            return true;
        }
    }

    free(families);
    return false;
}

/// Store the names of all available layers.
void vk_valididation_create(ValidationLayers *valid) {
    u32 idx;

    vkEnumerateInstanceLayerProperties(&valid->layer_count, NULL);
    valid->data = vmalloc(valid->layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&valid->layer_count, valid->data);

    valid->layers = vmalloc(valid->layer_count * sizeof(char *));
    for (idx = 0; idx < valid->layer_count; idx++)
        valid->layers[idx] = valid->data[idx].layerName;

    trace_array(valid->layers, valid->layer_count, "layers enabled: ");
}

void vk_valididation_destroy(ValidationLayers *valid) {
    if (get_log_level() < LOG_INFO)
        return;

    free(valid->layers);
    free(valid->data);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_handler(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                 VkDebugUtilsMessageTypeFlagsEXT type,
                 const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                 void *user_data) {

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
        get_log_level() >= LOG_INFO) {

        printf("\x1b[1;38;5;2m[v]\e[m %s\n", callback_data->pMessage);
        return VK_FALSE;
    }

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT &&
        get_log_level() >= LOG_WARN) {

        printf("\x1b[1;38;5;3m[v]\e[m %s\n", callback_data->pMessage);
        return VK_FALSE;
    }

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT ||
        severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        printf("\x1b[1;38;5;1m[v]\e[m %s\n", callback_data->pMessage);

        if (get_log_level() == LOG_TRACE)
            __asm__("int3");

        exit(1);
    }

    return VK_FALSE;
}

static PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
static PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;

bool vk_debugger_create(RenderContext *ctx) {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vk_debug_handler,
        .pUserData = NULL,
    };

    // find function loaded by `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
    CreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(
            ctx->instance, "vkCreateDebugUtilsMessengerEXT"));

    // find function loaded by `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
    DestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(
            ctx->instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (!CreateDebugUtilsMessengerEXT || !DestroyDebugUtilsMessengerEXT)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    return CreateDebugUtilsMessengerEXT(
        ctx->instance,
        &create_info,
        NULL,
        &ctx->messenger
    ) == VK_SUCCESS;
}

/// Create a valid swapchain present extent.
void create_swapchain_present_extent(RenderContext *ctx) {
    VkSurfaceCapabilitiesKHR *capabilities = &ctx->swapchain.capabilities;
    i32 width, height;

    SDL_Vulkan_GetDrawableSize(ctx->window, &width, &height);

    // wait for next event if window is minimized
    while (width == 0 || height == 0) {
        SDL_Vulkan_GetDrawableSize(ctx->window, &width, &height);
        SDL_WaitEvent(NULL);
    }

    // some window managers set currentExtent.width to u32::MAX for some reason
    // so we'll just make up a good resolution in this case
    if (capabilities->currentExtent.width == 0xFFFFFFFF) {
        ctx->dimensions.width =
            clamp((u32)width, capabilities->minImageExtent.width,
                  capabilities->maxImageExtent.width);

        ctx->dimensions.height =
            clamp((u32)height, capabilities->minImageExtent.height,
                  capabilities->maxImageExtent.height);
    } else {
        ctx->dimensions = capabilities->currentExtent;
    }
}

/// generate a method to interact with images (image views).
///
/// `img_view` is the result of creating an image view
bool vk_image_view_create(RenderContext *ctx,
                          VkImage img,
                          VkFormat format,
                          VkImageView *img_view) {

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };

    if (vkCreateImageView(ctx->driver, &view_info, NULL, img_view))
        return false;

    return true;
}

bool vk_swapchain_image_views_create(RenderContext *ctx) {
    u32 idx;
    bool success;
    SwapChainDescriptor *chain = &ctx->swapchain;

    chain->views = vmalloc(chain->image_count * sizeof(VkImageView));

    for (idx = 0; idx < chain->image_count; idx++) {
        success = vk_image_view_create(
            ctx,
            chain->images[idx],
            ctx->surface_format.format,
            &chain->views[idx]
        );

        if (!success) {
            for (u32 jdx = 0; jdx < idx; jdx++)
                vkDestroyImage(ctx->driver, chain->images[jdx], NULL);

            free(chain->views);
            return false;
        }
    }

    return true;
}

bool vk_framebuffers_create(RenderContext *ctx) {
    SwapChainDescriptor *chain = &ctx->swapchain;
    VkImageView attachments[1];

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .attachmentCount = 1,
        .width = ctx->dimensions.width,
        .height = ctx->dimensions.height,
        .layers = 1};

    chain->framebuffers = vmalloc(chain->image_count * sizeof(VkFramebuffer));

    for (u32 idx = 0; idx < chain->image_count; idx++) {
        attachments[0] = chain->views[idx];
        framebuffer_info.pAttachments = attachments;

        if (vkCreateFramebuffer(ctx->driver, &framebuffer_info, NULL,
                                &chain->framebuffers[idx]))
            return false;
    }

    return true;
}

// NOTE: may want to use `VK_IMAGE_USAGE_TRANSFER_DST_BIT` for image usage
// as it allows rendering to a seperate image first to perform
// post-processing

// NOTE: `oldSwapchain` can specify a backup swapchain for when the window
// get's resized or if the chain becomes invalid

// VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue
// families without explicit ownership transfers
//
// VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a
// time and ownership must be explicitly transferred before using it in
// another queue family. This option offers the best performance

/// Try to create a swapchain with at least one format.
bool vk_swapchain_create(RenderContext *ctx) {
    SwapChainDescriptor *chain = &ctx->swapchain;
    VkSurfaceCapabilitiesKHR *capabilities = &chain->capabilities;
    VkResult vk_fail;

    VkSurfaceFormatKHR fallback_surface_format = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ctx->surface,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    vk_fail = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        ctx->device,
        ctx->surface,
        capabilities
    );

    if (vk_fail)
        return false;

    create_info.preTransform = capabilities->currentTransform;

    // number of images to be held in the swapchain
    chain->image_count = capabilities->minImageCount + 2;
    create_info.minImageCount = chain->image_count;

    vk_fail = vkGetPhysicalDeviceSurfaceFormatsKHR(
        ctx->device,
        ctx->surface,
        &chain->format_count,
        NULL
    );

    if (chain->format_count == 0 || vk_fail)
        return false;

    chain->formats = vmalloc(chain->format_count * sizeof(VkSurfaceFormatKHR));
    vk_fail = vkGetPhysicalDeviceSurfaceFormatsKHR(
        ctx->device,
        ctx->surface,
        &chain->format_count,
        chain->formats
    );

    if (chain->format_count == 0 || vk_fail)
        return false;

    if (!try_preferred_swapchain_format(ctx)) {
        warn("couldnt find suitable swapchain format, using fallback");
        ctx->surface_format = fallback_surface_format;
    }

    create_info.imageFormat = ctx->surface_format.format;

    if (!try_preferred_present_mode(ctx, &create_info.presentMode))
        create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    create_swapchain_present_extent(ctx);
    create_info.imageExtent = ctx->dimensions;

    if (ctx->present_queue != ctx->queue) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = &ctx->queue_family_indices;
    }

    if (vkCreateSwapchainKHR(ctx->driver, &create_info, NULL, &chain->data))
        return false;

    vk_fail = vkGetSwapchainImagesKHR(
        ctx->driver,
        chain->data,
        &chain->format_count,
        NULL
    );

    if (vk_fail) {
        error("failed to read count of swapchain images");
        return false;
    }

    chain->images = vmalloc(chain->image_count * sizeof(VkImage));
    vk_fail = vkGetSwapchainImagesKHR(
        ctx->driver,
        chain->data,
        &chain->format_count,
        chain->images
    );

    if (vk_fail) {
        error("failed to get swapchain images");
        return false;
    }

    return true;
}

void vk_swapchain_destroy(RenderContext *ctx) {
    u32 idx;
    SwapChainDescriptor *chain = &ctx->swapchain;

    for (idx = 0; idx < chain->image_count; idx++)
        vkDestroyFramebuffer(ctx->driver, chain->framebuffers[idx], NULL);

    for (idx = 0; idx < chain->image_count; idx++)
        vkDestroyImageView(ctx->driver, chain->views[idx], NULL);

    vkDestroySwapchainKHR(ctx->driver, chain->data, NULL);

    free(chain->framebuffers);
    free(chain->images);
    free(chain->formats);
    free(chain->views);
}

bool vk_swapchain_recreate(RenderContext *ctx) {
    vkDeviceWaitIdle(ctx->driver);

    vk_swapchain_destroy(ctx);

    if (!vk_swapchain_create(ctx))
        return false;

    if (!vk_swapchain_image_views_create(ctx))
        return false;

    if (!vk_framebuffers_create(ctx))
        return false;

    return true;
}

// NOTE: can create multiple logical devices with different requirements
// for the same physical device

// NOTE: for now the present_queue and queue will be the same

/// Try to create a device, associated queue, present queue and surface.
bool vk_device_create(RenderContext *ctx) {
    f32 queue_priority = 1.0;
    const char *device_extensions[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkBool32 surface_supported = false;
    VkResult vk_fail;

    VkPhysicalDeviceFeatures device_features = {
        .samplerAnisotropy = VK_TRUE
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };

    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    // find a simple queue that can handle at least graphics for now
    if (!find_queue_families(ctx)) {
        warn("couldn't find any queue families");
        return false;
    }

    queue_create_info.queueFamilyIndex = ctx->queue_family_indices;
    device_create_info.pQueueCreateInfos = &queue_create_info;

    if (vkCreateDevice(ctx->device, &device_create_info, NULL, &ctx->driver)) {
        warn("failed to create driver");
        return false;
    }

    vkGetDeviceQueue(ctx->driver, ctx->queue_family_indices, 0, &ctx->queue);

    if (!SDL_Vulkan_CreateSurface(ctx->window, ctx->instance, &ctx->surface)) {
        warn("failed to create surface");
        return false;
    }

    vk_fail = vkGetPhysicalDeviceSurfaceSupportKHR(
        ctx->device,
        ctx->queue_family_indices,
        ctx->surface,
        &surface_supported
    );

    if (!surface_supported || vk_fail) {
        warn("selected queue doesn't support required surface");
        return false;
    }

    return true;
}

/// Try to setup a device that supports the required
/// features, extensions and swapchain.
bool vk_most_suitable_device_create(RenderContext *ctx) {
    u32 device_count, idx;
    VkPhysicalDevice *devices;
    VkPhysicalDeviceType preferred_device;

    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);

    if (device_count == 0)
        return false;

    devices = vmalloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    preferred_device = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // try to find a discrete GPU
    for (idx = 0; idx < device_count; idx++) {
        ctx->device = devices[idx];

        vkGetPhysicalDeviceProperties(ctx->device, &ctx->dev_prop);
        vkGetPhysicalDeviceMemoryProperties(ctx->device, &ctx->mem_prop);

        if (ctx->dev_prop.deviceType != preferred_device)
            continue;

        trace("GPU: %s", ctx->dev_prop.deviceName);

        if (!matches_device_requirements(ctx->device))
            continue;

        if (!vk_device_create(ctx))
            continue;

        if (!vk_swapchain_create(ctx))
            continue;

        free(devices);
        return true;
    }

    // try to find any GPU
    for (idx = 0; idx < device_count; idx++) {
        ctx->device = devices[idx];

        vkGetPhysicalDeviceProperties(ctx->device, &ctx->dev_prop);
        vkGetPhysicalDeviceMemoryProperties(ctx->device, &ctx->mem_prop);

        if (ctx->dev_prop.deviceType == preferred_device)
            continue;

        trace("GPU: %s", ctx->dev_prop.deviceName);

        if (!matches_device_requirements(ctx->device))
            continue;

        if (!vk_device_create(ctx))
            continue;

        if (!vk_swapchain_create(ctx))
            continue;

        free(devices);
        return true;
    }

    free(devices);
    return false;
}

bool vk_instance_create(RenderContext *ctx) {
    u32 ext_count, opt_count;
    const char **extensions;
    VkResult vk_fail;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_2,
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

#ifdef __APPLE__
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,

        .pfnUserCallback = vk_debug_handler,
    };

    if (!(extensions = get_required_extensions(ctx->window, &ext_count)))
        return false;

    instance_create_info.enabledExtensionCount = ext_count;
    instance_create_info.ppEnabledExtensionNames = extensions;

    if (get_log_level() > LOG_WARN) {
        ValidationLayers *valid = &ctx->validation;
        vk_valididation_create(valid);

        // enable all supported layers when for debugging
        instance_create_info.enabledLayerCount = valid->layer_count;
        instance_create_info.ppEnabledLayerNames = valid->layers;

        // attach debugger just for `vkDestroyInstance` and `vkCreateInstance`
        instance_create_info.pNext = &debug_create_info;
    }

    vk_fail = vkCreateInstance(&instance_create_info, NULL, &ctx->instance);

    free(get_optional_extensions(&opt_count));
    free(extensions);

    return vk_fail == VK_SUCCESS;
}

bool vk_shader_module_create(RenderContext *ctx, char *binary,
                             u32 binary_size, VkShaderModule *module) {

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary_size,
        .pCode = (const u32 *)binary
    };

    return vkCreateShaderModule(
        ctx->driver,
        &create_info,
        NULL,
        module
    ) == VK_SUCCESS;
}

bool vk_render_pass_create(RenderContext *ctx) {
    VkAttachmentDescription color_attachment = {
        .format = ctx->surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    // NOTE: one attachment can have multiple subpasses for post-processing

    // `pColorAttachments` refers to `layout(location = 0) out vec4 outColor`
    VkSubpassDescription subpass = {.colorAttachmentCount = 1,
                                    .pColorAttachments = &color_attachment_ref};

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pAttachments = &color_attachment,
        .attachmentCount = 1,
        .pSubpasses = &subpass,
        .subpassCount = 1,
        .pDependencies = &dependency,
        .dependencyCount = 1};

    return vkCreateRenderPass(
        ctx->driver,
        &render_pass_info,
        NULL,
        &ctx->render_pass
    ) == VK_SUCCESS;
}

// VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
//
// VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without
//
// reuse VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: the end vertex of every line is
// used as start vertex for the next line
//
// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: triangle from every 3 vertices
//
// without reuse VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: the second and third
// vertex of every triangle are used as first two vertices of the next
// triangle

// Normally, the vertices are loaded from the vertex buffer by index in
// sequential order, but with an element buffer you can specify the indices
// to use yourself. This allows you to perform optimizations like reusing
// vertices. If you set the primitiveRestartEnable member to VK_TRUE, then
// it's possible to break up lines and triangles in the _STRIP topology
// modes by using a special index of 0xFFFF or 0xFFFFFFFF.

bool vk_pipeline_create(RenderContext *ctx) {
    u32 vert_size, frag_size;
    char *vert_bin;
    char *frag_bin;
    bool success = true;

    VkPipelineShaderStageCreateInfo vert_shader_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_shader_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[2];

    VkPipelineDynamicStateCreateInfo dynamic_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    };

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vert_attr_descs[2] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, pos)
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, tex)
        }
    };

    // TODO: `pVertexBindingDescriptions` and `pVertexAttributeDescriptions`
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vert_attr_descs
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // `polygonMode` can be used with `VK_POLYGON_MODE_LINE` for wireframe
    // !!! does require specific GPU features
    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    VkPipelineMultisampleStateCreateInfo multisampling_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->desc_set_layout,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .subpass = 0, // index into the render pass for what subpass to use
        .basePipelineHandle = NULL, // other potential pipeline to use for faster creation
        .basePipelineIndex = -1, // of pipelines that share functionality
        .stageCount = 2,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisampling_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_create_info,
        .pDepthStencilState = NULL,
    };

    vert_bin = read_binary("./target/shader.vert.spv", &vert_size);
    frag_bin = read_binary("./target/shader.frag.spv", &frag_size);

    if (vert_bin == NULL || frag_bin == NULL) {
        error("failed to read shader source");
        return false;
    }

    success |= vk_shader_module_create(ctx, frag_bin, frag_size, &ctx->frag);
    success |= vk_shader_module_create(ctx, vert_bin, vert_size, &ctx->vert);

    free(vert_bin);
    free(frag_bin);

    if (!success) {
        error("failed to create shader module");
        return false;
    }

    vert_shader_info.module = ctx->vert;
    frag_shader_info.module = ctx->frag;

    shader_stages[0] = vert_shader_info;
    shader_stages[1] = frag_shader_info;
    pipeline_info.pStages = shader_stages;

    ctx->dynamic_states = vmalloc(2 * sizeof(VkDynamicState));
    ctx->dynamic_state_count = 2;
    dynamic_create_info.dynamicStateCount = ctx->dynamic_state_count;

    ctx->dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    ctx->dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    dynamic_create_info.pDynamicStates = ctx->dynamic_states;

    ctx->viewport.x = 0.0;
    ctx->viewport.y = 0.0;
    ctx->viewport.width = (float)ctx->dimensions.width;
    ctx->viewport.height = (float)ctx->dimensions.height;
    ctx->viewport.minDepth = 0.0;
    ctx->viewport.maxDepth = 1.0;
    viewport_info.pViewports = &ctx->viewport;

    ctx->scissor.offset.x = 0;
    ctx->scissor.offset.y = 0;
    ctx->scissor.extent = ctx->dimensions;
    viewport_info.pScissors = &ctx->scissor;

    success = vkCreatePipelineLayout(
        ctx->driver,
        &pipeline_layout_info,
        NULL,
        &ctx->pipeline_layout
    ) == VK_SUCCESS;

    if (!success) {
        error("failed to create pipeline layout");
        return false;
    }

    pipeline_info.layout = ctx->pipeline_layout;

    // `vkCreateGraphicsPipelines` takes a list of pipelines to create at once
    return vkCreateGraphicsPipelines(
        ctx->driver,
        NULL,
        1,
        &pipeline_info,
        NULL,
        &ctx->pipeline
    ) == VK_SUCCESS;
}

void vk_pipeline_destroy(RenderContext *ctx) {
    vkDestroyPipeline(ctx->driver, ctx->pipeline, NULL);
    vkDestroyPipelineLayout(ctx->driver, ctx->pipeline_layout, NULL);
    vkDestroyShaderModule(ctx->driver, ctx->vert, NULL);
    vkDestroyShaderModule(ctx->driver, ctx->frag, NULL);

    free(ctx->dynamic_states);
}

bool vk_descriptor_layouts_create(RenderContext *ctx) {
    VkDescriptorSetLayoutBinding sampler_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    // can take multiply bindings
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_layout_binding,
    };

    return vkCreateDescriptorSetLayout(
        ctx->driver,
        &layout_info,
        NULL,
        &ctx->desc_set_layout
    ) == VK_SUCCESS;
}

bool vk_descriptor_pool_create(RenderContext *ctx) {
    // pool big enough for a sampler
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_FRAMES_LOADED
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = MAX_FRAMES_LOADED * 3
    };

    return vkCreateDescriptorPool(
        ctx->driver,
        &pool_info,
        NULL,
        &ctx->desc_pool
    ) == VK_SUCCESS;
}

bool vk_descriptor_sets_create(RenderContext *ctx, Texture *tex) {
    u32 idx;
    VkDescriptorSetLayout desc_set_layouts[MAX_FRAMES_LOADED];

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx->desc_pool,
    };

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .sampler = ctx->sampler,
        .imageView = tex->view
    };

    VkWriteDescriptorSet desc_set = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info
    };

    // copy the same descriptor set layout for each frame we render
    for (idx = 0; idx < MAX_FRAMES_LOADED; idx++)
        desc_set_layouts[idx] = ctx->desc_set_layout;

    alloc_info.pSetLayouts = desc_set_layouts;
    alloc_info.descriptorSetCount = MAX_FRAMES_LOADED;

    if (vkAllocateDescriptorSets(ctx->driver, &alloc_info, tex->desc_sets)) {
        error("failed to allocate descriptor sets");
        return false;
    }

    // for every frame
    for (idx = 0; idx < MAX_FRAMES_LOADED; idx++) {
        desc_set.dstSet = tex->desc_sets[idx];

        // update the sampler
        vkUpdateDescriptorSets(
            ctx->driver,
            1,
            &desc_set,
            0,
            NULL
        );
    }

    return true;
}

void vk_descriptors_destroy(RenderContext *ctx) {
    vkDestroyDescriptorPool(ctx->driver, ctx->desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx->driver, ctx->desc_set_layout, NULL);
}

bool vk_cmd_pool_create(RenderContext *ctx) {
    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->queue_family_indices};

    return vkCreateCommandPool(
        ctx->driver,
        &create_info,
        NULL,
        &ctx->cmd_pool
    ) == VK_SUCCESS;
}

bool vk_cmd_buffers_alloc(RenderContext *ctx,
                          VkCommandBuffer *cmd_bufs,
                          u32 count) {

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->cmd_pool,
        .commandBufferCount = count,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    return vkAllocateCommandBuffers(
        ctx->driver,
        &alloc_info,
        cmd_bufs
    ) == VK_SUCCESS;
}

/// cmd_buf is the result of creating and beginning a command buffer
bool vk_cmd_oneshot_start(RenderContext *ctx, VkCommandBuffer *cmd_buf) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if (!vk_cmd_buffers_alloc(ctx, cmd_buf, 1)) {
        error("failed to allocate command buffer");
        return false;
    }

    if (vkBeginCommandBuffer(*cmd_buf, &begin_info)) {
        error("failed to begin command buffer");
        vkFreeCommandBuffers(ctx->driver, ctx->cmd_pool, 1, cmd_buf);
        return false;
    }

    return true;
}

bool vk_cmd_oneshot_end(RenderContext *ctx, VkCommandBuffer cmd_buf) {
    if (vkEndCommandBuffer(cmd_buf)) {
        error("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pCommandBuffers = &cmd_buf,
        .commandBufferCount = 1,
    };

    if (vkQueueSubmit(ctx->queue, 1, &submit_info, NULL)) {
        error("failed to submit command buffer to queue");
        return false;
    }

    if (vkQueueWaitIdle(ctx->queue)) {
        error("failed to wait for queue");
        return false;
    }

    vkFreeCommandBuffers(ctx->driver, ctx->cmd_pool, 1, &cmd_buf);

    return true;
}

bool vk_buffer_copy(RenderContext *ctx, VkBuffer dst, VkBuffer src,
                    VkDeviceSize size) {

    VkCommandBuffer cmd_buf;
    VkBufferCopy copy_region = {
        .size = size
    };

    if (!vk_cmd_oneshot_start(ctx, &cmd_buf))
        return false;

    vkCmdCopyBuffer(cmd_buf, src, dst, 1, &copy_region);

    if (!vk_cmd_oneshot_end(ctx, cmd_buf))
        return false;

    return true;
}

bool vk_buffer_create(RenderContext *ctx, VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags flags,
                      VkBuffer *buf, VkDeviceMemory *buf_mem) {

    VkMemoryRequirements mem_reqs;

    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
        .usage = usage,
    };

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    };

    if (vkCreateBuffer(ctx->driver, &buf_info, NULL, buf))
        return false;

    vkGetBufferMemoryRequirements(ctx->driver, *buf, &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = vk_find_memory_type(
        ctx,
        mem_reqs,
        flags
    );

    if (vkAllocateMemory(ctx->driver, &alloc_info, NULL, buf_mem)) {
        vkDestroyBuffer(ctx->driver, *buf, NULL);
        error("failed to allocate buffer memory");
        return false;
    }

    if (vkBindBufferMemory(ctx->driver, *buf, *buf_mem, 0)) {
        error("failed to bind buffer memory");
        vkDestroyBuffer(ctx->driver, *buf, NULL);
        vkFreeMemory(ctx->driver, *buf_mem, NULL);
        return false;
    }

    return true;
}

bool vk_buffer_copy_to_image(RenderContext *ctx, VkBuffer buf, VkImage img,
                             u32 width, u32 height) {

    VkCommandBuffer cmd_buf;

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };

    if (!vk_cmd_oneshot_start(ctx, &cmd_buf))
        return false;

    vkCmdCopyBufferToImage(
        cmd_buf,
        buf,
        img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (!vk_cmd_oneshot_end(ctx, cmd_buf))
        return false;

    return true;
}

bool vk_vertices_indices_copy(RenderContext *ctx, Object *obj) {
    void *data;
    VkDeviceSize buf_size;
    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_mem;
    bool success;

    /* -------------------initialize vertex buffers ------------------ */
    buf_size = sizeof(Vertex) * obj->vertices_count;

    success = vk_buffer_create(
        ctx,
        buf_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf,
        &staging_buf_mem
    );

    if (!success) {
        error("failed to create vertex staging buffer");
        return false;
    }

    if (vkMapMemory(ctx->driver, staging_buf_mem, 0, buf_size, 0, &data)) {
        error("failed to map vertex buffer memory");
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        return false;
    }

    memcpy(data, obj->vertices, (usize)buf_size);
    vkUnmapMemory(ctx->driver, staging_buf_mem);

    success = vk_buffer_create(
        ctx,
        buf_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &obj->vertices_buf,
        &obj->vertices_mem
    );

    if (!success) {
        error("failed to create vertex buffer");
        goto end;
    }

    if (!vk_buffer_copy(ctx, obj->vertices_buf, staging_buf, buf_size)) {
        error("failed to copy staging buf into vertex buf");
        goto end;
    }

    vkDestroyBuffer(ctx->driver, staging_buf, NULL);
    vkFreeMemory(ctx->driver, staging_buf_mem, NULL);

    /* -------------------initialize index buffers ------------------ */
    buf_size = sizeof(u16) * obj->indices_count;

    success = vk_buffer_create(
        ctx,
        buf_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf,
        &staging_buf_mem
    );

    if (!success) {
        error("failed to create index staging buffer");
        return false;
    }

    if (vkMapMemory(ctx->driver, staging_buf_mem, 0, buf_size, 0, &data)) {
        error("failed to map index buffer memory");
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        return false;
    }

    memcpy(data, obj->indices, (usize)buf_size);
    vkUnmapMemory(ctx->driver, staging_buf_mem);

    success = vk_buffer_create(
        ctx,
        buf_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &obj->indices_buf,
        &obj->indices_mem
    );

    if (!success) {
        error("failed to create index buffer");
        goto end;
    }

    if (!vk_buffer_copy(ctx, obj->indices_buf, staging_buf, buf_size)) {
        error("failed to copy staging buf into index buf");
        goto end;
    }

    vkDestroyBuffer(ctx->driver, staging_buf, NULL);
    vkFreeMemory(ctx->driver, staging_buf_mem, NULL);

    return true;

end:
    vkDestroyBuffer(ctx->driver, staging_buf, NULL);
    vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
    return false;
}

/// transform image data to a layout more memory cache friendly to the GPU
bool vk_image_layout_transition(RenderContext *ctx, VkImage img,
                                VkImageLayout old_layout,
                                VkImageLayout new_layout) {

    VkCommandBuffer cmd_buf;
    VkPipelineStageFlags src_stage, dst_stage;

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = img,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };

    if (!vk_cmd_oneshot_start(ctx, &cmd_buf))
        return false;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (!src_stage || !dst_stage) {
        error("unsupported layout transition");
        return false;
    }

    // stages docs:
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap7.html#synchronization-access-types-supported
    vkCmdPipelineBarrier(
        cmd_buf,
        src_stage, // stage before barrier
        dst_stage, // stage after barrier
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier
    );

    if (!vk_cmd_oneshot_end(ctx, cmd_buf))
        return false;

    return true;
}

bool vk_image_texture_create(RenderContext *ctx,
                             Texture *tex,
                             SDL_Surface *img) {
    VkMemoryRequirements mem_reqs;

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT,
        .extent.width = (u32)img->w,
        .extent.height = (u32)img->h
    };

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    };

    if (vkCreateImage(ctx->driver, &image_info, NULL, &tex->image))
        return false;

    vkGetImageMemoryRequirements(ctx->driver, tex->image, &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = vk_find_memory_type(
        ctx,
        mem_reqs,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(ctx->driver, &alloc_info, NULL, &tex->mem)) {
        error("failed to allocate image texture");
        vkDestroyImage(ctx->driver, tex->image, NULL);
        return false;
    }

    if (vkBindImageMemory(ctx->driver, tex->image, tex->mem, 0)) {
        error("failed to bind image texture memory");
        vkDestroyImage(ctx->driver, tex->image, NULL);
        vkFreeMemory(ctx->driver, tex->mem, NULL);
        return false;
    }

    return true;
}

bool vk_image_create(RenderContext *ctx, Texture *tex, const char *path) {
    VkDeviceSize img_size;
    SDL_Surface *img;
    SDL_Surface *conv_img;
    VkDeviceMemory staging_buf_mem;
    VkBuffer staging_buf;
    void *data;
    bool success;

    if (!(img = SDL_LoadBMP(path))) {
        error("failed to load image: '%s'", path);
        return false;
    }

    /* ------- convert surface to `VK_FORMAT_B8G8R8A8_SRGB` ------- */
    conv_img = SDL_ConvertSurfaceFormat(
        img,
        SDL_PIXELFORMAT_BGRA32,
        0
    );

    if (conv_img == NULL) {
        error("failed to convert image to 'VK_FORMAT_B8G8R8A8_SRGB'");
        return false;
    }

    SDL_FreeSurface(img);
    img = conv_img;
    /* ------------------------------------------------------------ */

    img_size = img->w * img->h * img->format->BytesPerPixel;

    success = vk_buffer_create(
        ctx,
        img_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf,
        &staging_buf_mem
    );

    if (!success) {
        SDL_FreeSurface(img);
        error("failed to create image buffer");
        return false;
    }

    if (vkMapMemory(ctx->driver, staging_buf_mem, 0, img_size, 0, &data)) {
        error("failed to map vertex buffer memory");
        SDL_FreeSurface(img);
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        return false;
    }

    memcpy(data, img->pixels, img_size);
    vkUnmapMemory(ctx->driver, staging_buf_mem);

    if (!vk_image_texture_create(ctx, tex, img)) {
        error("failed to create image texture");
        SDL_FreeSurface(img);
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
        return false;
    }

    // change layout from any undefined data to an optimized format
    success = vk_image_layout_transition(
        ctx,
        tex->image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    if (!success) {
        error("failed to transition image to optimal layout");
        SDL_FreeSurface(img);
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
        vkDestroyImage(ctx->driver, tex->image, NULL);
        return false;
    }

    success = vk_buffer_copy_to_image(
        ctx,
        staging_buf,
        tex->image,
        img->w,
        img->h
    );

    if (!success) {
        error("failed to copy staging buffer into VkImage");
        SDL_FreeSurface(img);
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
        vkDestroyImage(ctx->driver, tex->image, NULL);
        return false;
    }

    SDL_FreeSurface(img);

    success = vk_image_layout_transition(
        ctx,
        tex->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    if (!success) {
        error("failed to transition image to a optimal read-only layout");
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
        vkDestroyImage(ctx->driver, tex->image, NULL);
        return false;
    }

    success = vk_image_view_create(
        ctx,
        tex->image,
        VK_FORMAT_B8G8R8A8_SRGB,
        &tex->view
    );

    if (!success) {
        error("failed to create image texture view");
        vkDestroyBuffer(ctx->driver, staging_buf, NULL);
        vkFreeMemory(ctx->driver, staging_buf_mem, NULL);
        vkDestroyImage(ctx->driver, tex->image, NULL);
        return false;
    }

    vkDestroyBuffer(ctx->driver, staging_buf, NULL);
    vkFreeMemory(ctx->driver, staging_buf_mem, NULL);

    return true;
}

bool vk_image_sampler_create(RenderContext *ctx) {
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = ctx->dev_prop.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0,
        .minLod = 0.0,
        .maxLod = 0.0,
    };

    return vkCreateSampler(
        ctx->driver,
        &sampler_info,
        NULL,
        &ctx->sampler
    ) == VK_SUCCESS;
}

bool vk_sync_primitives_create(RenderContext *ctx) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (u32 idx = 0; idx < MAX_FRAMES_LOADED; idx++) {
        VkResult vk_fail = VK_SUCCESS;
        Synchronization *sync = &ctx->sync[idx];

        vk_fail |= vkCreateSemaphore(
            ctx->driver,
            &semaphore_info,
            NULL,
            &sync->images_available
        );

        vk_fail |= vkCreateSemaphore(
            ctx->driver,
            &semaphore_info,
            NULL,
            &sync->renders_finished
        );

        vk_fail |= vkCreateFence(
            ctx->driver,
            &fence_info,
            NULL,
            &sync->renderers_busy
        );

        if (vk_fail)
            return false;
    }

    return true;
}

void vk_sync_primitives_destroy(RenderContext *ctx) {
    for (u32 idx = 0; idx < MAX_FRAMES_LOADED; idx++) {
        Synchronization *sync = &ctx->sync[idx];

        vkDestroySemaphore(ctx->driver, sync->images_available, NULL);
        vkDestroySemaphore(ctx->driver, sync->renders_finished, NULL);
        vkDestroyFence(ctx->driver, sync->renderers_busy, NULL);
    }
}

/*
 * GAME IS MADE OUT OF A GRID OF 16x9
 *
 * SO PLAYER NEEDS TO BE A BLOCK
 *
 * HIS WIDTH IS 2 / 16
 * HIS HEIGHT IS 2 / 9
 */

void vk_engine_create(RenderContext *ctx) {
    ctx->frame = 0;
    ctx->object_count = 0;
    ctx->object_alloc_count = 0;

    float back[4][2] = { {-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0} };
    float guy[4][2] = { {-2.0/9.0, -0.125}, {2.0/9.0, -0.125}, {2.0/9.0, 0.125}, {-2.0/9.0, 0.125} };

    convert_pos_to_aspect_ratio(guy);

    if (!vk_instance_create(ctx))
        panic("failed to create instance");

    if (!vk_debugger_create(ctx))
        panic("failed to attach debugger");

    if (!vk_most_suitable_device_create(ctx))
        panic("failed to setup any GPU");

    if (!vk_swapchain_image_views_create(ctx))
        panic("failed to create image views");

    if (!vk_render_pass_create(ctx))
        panic("failed to create render pass");

    if (!vk_descriptor_layouts_create(ctx))
        panic("failed to create descriptor set layout");

    if (!vk_pipeline_create(ctx))
        panic("failed to create a pipeline");

    if (!vk_framebuffers_create(ctx))
        panic("failed to create framebuffer");

    if (!vk_cmd_pool_create(ctx))
        panic("failed to create command pool");

    if (!vk_image_sampler_create(ctx))
        panic("failed to create image sampler");

    if (!vk_descriptor_pool_create(ctx))
        panic("failed to create descriptor pool");

    if (!vk_cmd_buffers_alloc(ctx, ctx->cmd_bufs, MAX_FRAMES_LOADED))
        panic("failed to create command buffer");

    if (!vk_sync_primitives_create(ctx))
        panic("failed to create synchronization primitives");

    if (!object_create(ctx, back, "./assets/room_base.bmp"))
        panic("failed to create object");

    if (!object_create(ctx, guy, "./assets/guy.bmp"))
        panic("failed to create object");

    info("vulkan engine created");
}


// FIXME: layers appear to be unloading twice
void vk_engine_destroy(RenderContext *ctx) {
    vkDeviceWaitIdle(ctx->driver);

    objects_destroy(ctx);

    vk_sync_primitives_destroy(ctx);
    vkDestroyCommandPool(ctx->driver, ctx->cmd_pool, NULL);
    vkDestroyDescriptorPool(ctx->driver, ctx->desc_pool, NULL);
    vk_pipeline_destroy(ctx);
    vkDestroyDescriptorSetLayout(ctx->driver, ctx->desc_set_layout, NULL);
    vkDestroyRenderPass(ctx->driver, ctx->render_pass, NULL);
    vk_swapchain_destroy(ctx);
    vkDestroySampler(ctx->driver, ctx->sampler, NULL);
    vkDestroyDevice(ctx->driver, NULL);
    DestroyDebugUtilsMessengerEXT(ctx->instance, ctx->messenger, NULL);
    vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    vkDestroyInstance(ctx->instance, NULL);
    vk_valididation_destroy(&ctx->validation);

    info("vulkan engine destroyed");
}

/* `vk_engine_render` and `vk_record_cmd_buffer` basic overview.
 *
 *
 * 2. Acquire an image from the swap chain
 * 3. Record a command buffer which draws the scene onto that image
 * 4. Submit the recorded command buffer
 * 5. Present the swap chain image */

bool vk_record_cmd_buffer(RenderContext *ctx, u32 img_idx) {
    VkDeviceSize offsets[1] = {0};
    VkCommandBuffer cmd_buf = ctx->cmd_bufs[ctx->frame];

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    // black with 100% opacity
    VkClearValue clear_color = {{{0.0, 0.0, 0.0, 0.0}}};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->swapchain.framebuffers[img_idx],
        .renderArea.offset = {0, 0},
        .renderArea.extent = ctx->dimensions,
        .pClearValues = &clear_color,
        .clearValueCount = 1,
    };

    if (vkBeginCommandBuffer(cmd_buf, &begin_info))
        return false;

    /* ------------------------ render pass ------------------------ */
    vkCmdBeginRenderPass(
        cmd_buf,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

    vkCmdBindPipeline(
        cmd_buf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx->pipeline
    );

    // setting necessary dynamic state
    vkCmdSetViewport(cmd_buf, 0, 1, &ctx->viewport);
    vkCmdSetScissor(cmd_buf, 0, 1, &ctx->scissor);

    // draw every object, it's vertices and indices.
    for (u32 idx = 0; idx < ctx->object_count; idx++) {
        Object *obj = &ctx->objects[idx];

        vkCmdBindDescriptorSets(
            cmd_buf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx->pipeline_layout,
            0,
            1,
            &obj->texture.desc_sets[ctx->frame],
            0,
            NULL
        );

        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &obj->vertices_buf, offsets);
        vkCmdBindIndexBuffer(cmd_buf, obj->indices_buf, 0, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(cmd_buf, obj->indices_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd_buf);

    return vkEndCommandBuffer(cmd_buf) == VK_SUCCESS;
}

void vk_engine_render(RenderContext* ctx) {
    u32 img_idx;
    VkResult vk_fail;
    Synchronization *sync = &ctx->sync[ctx->frame];
    VkSemaphore wait_semaphores[1] = { sync->images_available };
    VkSemaphore signal_semaphores[1] = { sync->renders_finished };

    VkPipelineStageFlags wait_stages[1] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pWaitSemaphores = wait_semaphores,
        .waitSemaphoreCount = 1,
        .pWaitDstStageMask = wait_stages,
        .pCommandBuffers = &ctx->cmd_bufs[ctx->frame],
        .commandBufferCount = 1,
        .pSignalSemaphores = signal_semaphores,
        .signalSemaphoreCount = 1
    };

    VkSwapchainKHR swapchains[1] = { ctx->swapchain.data };

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pWaitSemaphores = signal_semaphores,
        .waitSemaphoreCount = 1,
        .pSwapchains = swapchains,
        .swapchainCount = 1,
        .pImageIndices = &img_idx
    };

    vkWaitForFences(
        ctx->driver,
        1,
        &sync->renderers_busy,
        VK_TRUE,
        UINT64_MAX
    );

    vk_fail = vkAcquireNextImageKHR(
        ctx->driver,
        ctx->swapchain.data,
        UINT64_MAX,
        sync->images_available,
        VK_NULL_HANDLE,
        &img_idx
    );

    if (vk_fail == VK_SUBOPTIMAL_KHR || vk_fail == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!vk_swapchain_recreate(ctx))
            warn("failed to recreate swapchain");
        return;
    }

    if (vk_fail) {
        warn("failed to acquire next image in swapchain");
        ctx->frame = (ctx->frame + 1) % MAX_FRAMES_LOADED;
        return;
    }

    vkResetFences(ctx->driver, 1, &sync->renderers_busy);
    vkResetCommandBuffer(ctx->cmd_bufs[ctx->frame], 0);

    vk_record_cmd_buffer(ctx, img_idx);

    if (vkQueueSubmit(ctx->queue, 1, &submit_info, sync->renderers_busy)) {
        error("failed to submit command buffer to queue");
        ctx->frame = (ctx->frame + 1) % MAX_FRAMES_LOADED;
        return;
    }

    if (vkQueuePresentKHR(ctx->queue, &present_info)) {
        error("failed to present queue");
        ctx->frame = (ctx->frame + 1) % MAX_FRAMES_LOADED;
        return;
    }
}
