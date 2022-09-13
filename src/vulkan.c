#include "render.h"

#include <stdlib.h>
#include <SDL2/SDL_vulkan.h>

const char** get_required_extensions(SDL_Window *window, u32 *count) {
    const char **extensions;

    SDL_Vulkan_GetInstanceExtensions(window, count, NULL);
    extensions = malloc((*count + 2) * sizeof(char*));
    SDL_Vulkan_GetInstanceExtensions(window, count, extensions);

    // MacOS requires the `VK_KHR_PORTABILITY_subset` extension
#ifdef __APPLE__
    extensions[(*count)++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif
    extensions[(*count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    trace_array(extensions, *count, "extensions enabled: ");

    return extensions;
}

const char** get_optional_extensions(u32 *count) {
    u32 idx;
    VkExtensionProperties* extensions;
    const char** extensions_names;

    vkEnumerateInstanceExtensionProperties(NULL, count, NULL);

    extensions = malloc(*count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, count, extensions);

    extensions_names = malloc(*count * sizeof(char*));
    for (idx = 0; idx < *count; idx++)
        extensions_names[idx] = extensions[idx].extensionName;

    trace_array(extensions_names, *count, "available extensions: ");
    free(extensions);

    return extensions_names;
}

bool matches_device_requirements(VkPhysicalDevice device) {
    u32 count, idx;
    VkResult extension_fail;
    VkPhysicalDeviceFeatures features;
    VkExtensionProperties* extensions;
    bool required_extensions_found;

    vkGetPhysicalDeviceFeatures(device, &features);

    extension_fail = vkEnumerateDeviceExtensionProperties(
        device,
        NULL,
        &count,
        NULL
    );

    extensions = malloc(count * sizeof(VkExtensionProperties));
    extension_fail = vkEnumerateDeviceExtensionProperties(
        device,
        NULL,
        &count,
        extensions
    );

    if (extension_fail)
        return false;

    required_extensions_found = false;
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
        const char** extension_names = malloc(count * sizeof(char*));

        for (idx = 0; idx < count; idx++)
            extension_names[idx] = extensions[idx].extensionName;

        trace_array(extension_names, count, "available device extensions: ");
        free(extension_names);
    }

    free(extensions);
    return features.geometryShader;
}

// Sets correct present mode on success. In the case of failing to find the
// preferred present mode, the present mode is left untouched.
bool try_preferred_present_mode(RenderContext *ctx,
                                VkPresentModeKHR *present_mode) {
    u32 count, idx;
    VkResult present_support_result;
    VkPresentModeKHR* present_modes;

    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        ctx->device,
        ctx->surface,
        &count,
        NULL
    );

    present_modes = malloc(count * sizeof(VkPresentModeKHR*));
    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        ctx->device,
        ctx->surface,
        &count,
        present_modes
    );

    if (present_support_result || count == 0) {
        free(present_modes);
        return false;
    }

    if (get_log_level() == LOG_TRACE) {
        const char** names = malloc(count * sizeof(char*));

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
bool try_preferred_swapchain_format(RenderContext* ctx) {
    u32 idx;
    VkSurfaceFormatKHR surface_format;

    for (idx = 0; idx < ctx->swapchain.format_count; idx++) {
        surface_format = ctx->swapchain.formats[idx];

        if (ctx->swapchain.formats[idx].format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            ctx->surface_format = surface_format;
            return true;
        }
    }

    return false;
}

/// Find a queue family that supports graphics.
bool find_queue_families(RenderContext *ctx) {
    u32 count, idx;
    VkQueueFamilyProperties* families;

    vkGetPhysicalDeviceQueueFamilyProperties(ctx->device, &count, NULL);
    families = malloc(count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->device, &count, families);

    for (idx = 0; idx < count; idx++) {
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
    valid->data = malloc(valid->layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&valid->layer_count, valid->data);

    valid->layers = malloc(valid->layer_count * sizeof(char*));
    for (idx = 0; idx < valid->layer_count; idx++)
        valid->layers[idx] = valid->data[idx].layerName;

    trace_array(valid->layers, valid->layer_count, "layers enabled: ");
}

void vk_valididation_destroy(ValidationLayers *valid) {
    free(valid->layers);
    free(valid->data);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_handler(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    char* msg = (char*)callback_data->pMessage;

    if (strncmp(msg, "Validation Error: ", 18) == 0)
        msg += 18;

    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            printf("\x1b[1;38;5;4m[v]\e[m %s\n", msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            printf("\x1b[1;38;5;2m[v]\e[m %s\n", msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            printf("\x1b[1;38;5;3m[v]\e[m %s\n", msg);
            break;
        default:
            printf("\x1b[1;38;5;1m[v]\e[m %s\n", msg);
            exit(1);
    }

    return VK_FALSE;
}

static PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
static PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;

bool vk_debugger_create(RenderContext *ctx) {
    if (get_log_level() < LOG_INFO)
        return true;

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
    CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)(
        vkGetInstanceProcAddr(ctx->instance, "vkCreateDebugUtilsMessengerEXT")
    );

    // find function loaded by `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
    DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)(
        vkGetInstanceProcAddr(ctx->instance, "vkDestroyDebugUtilsMessengerEXT")
    );

    if (!CreateDebugUtilsMessengerEXT || !DestroyDebugUtilsMessengerEXT)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    return CreateDebugUtilsMessengerEXT(
        ctx->instance,
        &create_info,
        NULL,
        &ctx->messenger
    ) == VK_SUCCESS;
}

void vk_debugger_destroy(RenderContext *ctx) {
    if (get_log_level() < LOG_INFO)
        return;

    DestroyDebugUtilsMessengerEXT(ctx->instance, ctx->messenger, NULL);
}

/// Create a valid swapchain present extent.
void create_swapchain_present_extent(RenderContext *ctx) {
    VkSurfaceCapabilitiesKHR *capabilities = &ctx->swapchain.capabilities;
    i32 width, height;

    SDL_Vulkan_GetDrawableSize(
        ctx->window,
        &width,
        &height
    );

    // wait for next event if window is minimized
    while (width == 0 || height == 0) {
        SDL_Vulkan_GetDrawableSize(
            ctx->window,
            &width,
            &height
        );

        SDL_WaitEvent(NULL);
    }

    // some window managers set currentExtent.width to u32::MAX for some reason
    // so we'll just make up a good resolution in this case
    if (capabilities->currentExtent.width == 0xFFFFFFFF) {
        ctx->dimensions.width = clamp(
            (u32)width,
            capabilities->minImageExtent.width,
            capabilities->maxImageExtent.width
        );

        ctx->dimensions.height = clamp(
            (u32)height,
            capabilities->minImageExtent.height,
            capabilities->maxImageExtent.height
        );
    } else {
        ctx->dimensions = capabilities->currentExtent;
    }
}

bool vk_image_views_create(RenderContext *ctx) {
    u32 idx;
    SwapChainDescriptor *chain = &ctx->swapchain;

    chain->views = malloc(chain->image_count * sizeof(VkImageView));

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = ctx->surface_format.format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };

    for (idx = 0; idx < chain->image_count; idx++) {
        create_info.image = chain->images[idx];
        if (vkCreateImageView(ctx->driver, &create_info, NULL, &chain->views[idx]))
            return false;
    }

    return true;
}

bool vk_framebuffers_create(RenderContext *ctx) {
    u32 idx;
    SwapChainDescriptor *chain = &ctx->swapchain;
    VkImageView attachments[1];
    VkResult framebuffer_fail;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .attachmentCount = 1,
        .width = ctx->dimensions.width,
        .height = ctx->dimensions.height,
        .layers = 1
    };

    chain->framebuffers = malloc(chain->image_count * sizeof(VkFramebuffer));

    for (idx = 0; idx < chain->image_count; idx++) {
        attachments[0] = chain->views[idx];
        framebuffer_info.pAttachments = attachments;

        framebuffer_fail = vkCreateFramebuffer(
            ctx->driver,
            &framebuffer_info,
            NULL,
            &chain->framebuffers[idx]
        );

        if (framebuffer_fail)
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
    SwapChainDescriptor* chain = &ctx->swapchain;
    VkSurfaceCapabilitiesKHR* capabilities = &chain->capabilities;
    VkResult vk_fail;

    VkSurfaceFormatKHR fallback_surface_format = {
        .format = VK_FORMAT_UNDEFINED,
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

    chain->formats = malloc(chain->format_count * sizeof(VkSurfaceFormatKHR));
    vk_fail = vkGetPhysicalDeviceSurfaceFormatsKHR(
        ctx->device,
        ctx->surface,
        &chain->format_count,
        chain->formats
    );

    if (chain->format_count == 0 || vk_fail)
        return false;

    if (!try_preferred_swapchain_format(ctx))
        ctx->surface_format = fallback_surface_format;

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

    chain->images = malloc(chain->image_count * sizeof(VkImage));
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
    SwapChainDescriptor* chain = &ctx->swapchain;

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

    if (!vk_image_views_create(ctx))
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
    const char* device_extensions[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkBool32 surface_supported = false;
    VkResult vk_fail;

    VkPhysicalDeviceFeatures device_features = {};
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
    VkPhysicalDevice* devices;
    VkPhysicalDeviceType preferred_device;
    VkPhysicalDeviceProperties properties;

    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);

    if (device_count == 0)
        return false;

    devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    preferred_device = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // try to find a discrete GPU
    for (idx = 0; idx < device_count; idx++) {
        ctx->device = devices[idx];

        vkGetPhysicalDeviceProperties(ctx->device, &properties);
        if (properties.deviceType != preferred_device)
            continue;

        trace("GPU: %s", properties.deviceName);

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

        vkGetPhysicalDeviceProperties(ctx->device, &properties);
        if (properties.deviceType == preferred_device)
            continue;

        trace("GPU: %s", properties.deviceName);

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
    const char** extensions = get_required_extensions(ctx->window, &ext_count);
    VkResult vk_fail;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = extensions,
    };

#ifdef __APPLE__
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,

        .pfnUserCallback = vk_debug_handler,
    };

    if (get_log_level() == LOG_TRACE) {
        ValidationLayers* valid = &ctx->validation;
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

VkResult vk_shader_module_create(RenderContext *ctx,
                                 char* binary,
                                 u32 binary_size,
                                 VkShaderModule *module) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary_size,
        .pCode = (u32*)binary
    };

    return vkCreateShaderModule(ctx->driver, &create_info, NULL, module);
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
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // NOTE: one attachment can have multiple subpasses for post-processing

    // `pColorAttachments` refers to `layout(location = 0) out vec4 outColor`
    VkSubpassDescription subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref
    };

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
        .dependencyCount = 1
    };

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
    char* vert_bin = read_binary("./target/shaders/shader.vert.spv", &vert_size);
    char* frag_bin = read_binary("./target/shaders/shader.frag.spv", &frag_size);
    VkResult vk_fail = VK_SUCCESS;

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

    // TODO: `pVertexBindingDescriptions` and `pVertexAttributeDescriptions`
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
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
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .subpass = 0, // index into the render pass for what subpass to use
        .basePipelineHandle = NULL, // other potential pipeline to use for faster creation
        .basePipelineIndex = -1,    // of pipelines that share functionality
        .pStages = shader_stages,
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

    if (vert_bin == NULL || frag_bin == NULL) {
        error("failed to read shader source");
        return false;
    }

    vk_fail |= vk_shader_module_create(ctx, frag_bin, frag_size, &ctx->frag);
    vk_fail |= vk_shader_module_create(ctx, vert_bin, vert_size, &ctx->vert);

    free(vert_bin);
    free(frag_bin);

    if (vk_fail) {
        error("failed to create shader module");
        return false;
    }

    vert_shader_info.module = ctx->vert;
    frag_shader_info.module = ctx->frag;

    shader_stages[0] = vert_shader_info;
    shader_stages[1] = frag_shader_info;

    ctx->dynamic_states = malloc(2 * sizeof(VkDynamicState));
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

    vk_fail = vkCreatePipelineLayout(
        ctx->driver,
        &pipeline_layout_info,
        NULL,
        &ctx->pipeline_layout
    );

    if (vk_fail)
        return false;

    pipeline_info.layout = ctx->pipeline_layout;

    // NOTE: `vkCreateGraphicsPipelines` takes a list of pipelines to create at once
    return vkCreateGraphicsPipelines(
        ctx->driver,
        VK_NULL_HANDLE,
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

bool vk_cmd_pool_create(RenderContext *ctx) {
    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->queue_family_indices
    };

    return vkCreateCommandPool(
        ctx->driver,
        &create_info,
        NULL,
        &ctx->cmd_pool
    ) == VK_SUCCESS;
}

bool vk_cmd_buffer_alloc(RenderContext *ctx) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->cmd_pool,
        .commandBufferCount = MAX_FRAMES_LOADED,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    return vkAllocateCommandBuffers(
        ctx->driver,
        &alloc_info,
        ctx->cmd_buffers
    ) == VK_SUCCESS;
}

bool vk_sync_primitives_create(RenderContext *ctx) {
    u32 idx;
    VkResult vk_fail = VK_SUCCESS;
    Synchronization *sync;

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (idx = 0; idx < MAX_FRAMES_LOADED; idx++) {
        sync = &ctx->sync[idx];

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
    u32 idx = 0;
    Synchronization *sync;

    for (idx = 0; idx < MAX_FRAMES_LOADED; idx++) {
        sync = &ctx->sync[idx];

        vkDestroySemaphore(ctx->driver, sync->images_available, NULL);
        vkDestroySemaphore(ctx->driver, sync->renders_finished, NULL);
        vkDestroyFence(ctx->driver, sync->renderers_busy, NULL);
    }
}

bool vk_record_cmd_buffer(RenderContext *ctx, u32 image_idx) {
    VkCommandBuffer frame_cmd_buffer = ctx->cmd_buffers[ctx->frame];

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    // black with 100% opacity
    VkClearValue clear_color = {{{0.0, 0.0, 0.0, 0.0}}};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->swapchain.framebuffers[image_idx],
        .renderArea.offset = {0, 0},
        .renderArea.extent = ctx->dimensions,
        .pClearValues = &clear_color,
        .clearValueCount = 1,
    };

    if (vkBeginCommandBuffer(frame_cmd_buffer, &begin_info))
        return false;

    /* ------------------------ render pass ------------------------ */

    vkCmdBeginRenderPass(
        frame_cmd_buffer,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

    vkCmdBindPipeline(
        frame_cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx->pipeline
    );

    // setting necessary dynamic state
    vkCmdSetViewport(frame_cmd_buffer, 0, 1, &ctx->viewport);
    vkCmdSetScissor(frame_cmd_buffer, 0, 1, &ctx->scissor);

    vkCmdDraw(frame_cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(frame_cmd_buffer);

    /* ------------------------------------------------------------- */

    return vkEndCommandBuffer(frame_cmd_buffer) == VK_SUCCESS;
}

void vk_engine_create(RenderContext *ctx) {
    if (!vk_instance_create(ctx))
        panic("failed to create instance");

    if (!vk_debugger_create(ctx))
        panic("failed to attach debugger");

    if (!vk_most_suitable_device_create(ctx))
        panic("failed to setup any GPU");

    if (!vk_image_views_create(ctx))
        panic("failed to create image views");

    if (!vk_render_pass_create(ctx))
        panic("failed to create render pass");

    if (!vk_pipeline_create(ctx))
        panic("failed to create a pipeline");

    if (!vk_framebuffers_create(ctx))
        panic("failed to create framebuffer");

    if (!vk_cmd_pool_create(ctx))
        panic("failed to create command pool");

    if (!vk_cmd_buffer_alloc(ctx))
        panic("failed to create command buffer");

    if (!vk_sync_primitives_create(ctx))
        panic("failed to create synchronization primitives");

    ctx->frame = 0;

    info("vulkan engine created");
}

// FIXME: layers appear to be unloading twice
void vk_engine_destroy(RenderContext *ctx) {
    vkDeviceWaitIdle(ctx->driver);

    vk_sync_primitives_destroy(ctx);
    vkDestroyCommandPool(ctx->driver, ctx->cmd_pool, NULL);
    vk_pipeline_destroy(ctx);
    vkDestroyRenderPass(ctx->driver, ctx->render_pass, NULL);
    vk_swapchain_destroy(ctx);
    vkDestroyDevice(ctx->driver, NULL);
    vk_debugger_destroy(ctx);
    vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    vkDestroyInstance(ctx->instance, NULL);

    if (get_log_level() == LOG_TRACE) {
        vk_valididation_destroy(&ctx->validation);
    }

    info("vulkan engine destroyed");
}

// 1. Wait for the previous frame to finish
// 2. Acquire an image from the swap chain
// 3. Record a command buffer which draws the scene onto that image
// 4. Submit the recorded command buffer
// 5. Present the swap chain image

void vk_engine_render(RenderContext* ctx) {
    u32 image_idx;
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
        .pCommandBuffers = &ctx->cmd_buffers[ctx->frame],
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
        .pImageIndices = &image_idx
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
        &image_idx
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
    vkResetCommandBuffer(ctx->cmd_buffers[ctx->frame], 0);

    vk_record_cmd_buffer(ctx, image_idx);

    if (vkQueueSubmit(ctx->queue, 1, &submit_info, sync->renderers_busy)) {
        warn("failed to submit queue tasks");
        ctx->frame = (ctx->frame + 1) % MAX_FRAMES_LOADED;
        return;
    }

    if (vkQueuePresentKHR(ctx->queue, &present_info)) {
        warn("failed to present queue");
        ctx->frame = (ctx->frame + 1) % MAX_FRAMES_LOADED;
        return;
    }
}
