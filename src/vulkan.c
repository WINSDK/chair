#include "render.h"

#include <stdlib.h>
#include <SDL2/SDL_vulkan.h>

const char** get_required_extensions(SDL_Window *window, u32 *count) {
    SDL_Vulkan_GetInstanceExtensions(window, count, NULL);

    const char **extensions = malloc((*count + 2) * sizeof(char*));
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
    vkEnumerateInstanceExtensionProperties(NULL, count, NULL);

    VkExtensionProperties* extensions = malloc(*count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, count, extensions);

    const char** extensions_names = malloc(*count * sizeof(char*));
    for (u32 idx = 0; idx < *count; idx++)
        extensions_names[idx] = extensions[idx].extensionName;

    trace_array(extensions_names, *count, "available extensions: ");
    free(extensions);

    return extensions_names;
}

bool matches_device_requirements(VkPhysicalDevice device) {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    u32 count;
    VkResult extension_fail;
    extension_fail = vkEnumerateDeviceExtensionProperties(
        device,
        NULL,
        &count,
        NULL
    );

    VkExtensionProperties* extensions = malloc(count * sizeof(VkExtensionProperties));
    extension_fail = vkEnumerateDeviceExtensionProperties(
        device,
        NULL,
        &count,
        extensions
    );

    if (extension_fail)
        return false;

    bool required_extensions_found = false;
    for (u32 idx = 0; idx < count; idx++) {
        const char* name = extensions[idx].extensionName;

        if (strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            required_extensions_found = true;
    }

    if (!required_extensions_found) {
        free(extensions);
        return false;
    }

    if (get_log_level() == LOG_TRACE) {
        const char** extension_names = malloc(count * sizeof(char*));

        for (u32 idx = 0; idx < count; idx++)
            extension_names[idx] = extensions[idx].extensionName;

        trace_array(extension_names, count, "available device extensions: ");
        free(extension_names);
    }

    free(extensions);
    return features.geometryShader;
}

// Sets correct present mode on success. In the case of failing to find the
// preferred present mode, the present mode is left untouched.
bool try_preferred_present_mode(RenderContext *context) {
    u32 count = 0;
    VkResult present_support_result;
    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device,
        context->surface,
        &count,
        NULL
    );

    VkPresentModeKHR* present_modes = malloc(count * sizeof(VkPresentModeKHR*));
    present_support_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device,
        context->surface,
        &count,
        present_modes
    );

    if (present_support_result || count == 0) {
        free(present_modes);
        return false;
    }

    if (get_log_level() == LOG_TRACE) {
        const char** names = malloc(count * sizeof(char*));

        for (u32 idx = 0; idx < count; idx++) {
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
    for (u32 idx = 0; idx < count; idx++) {
        if (present_modes[idx] == VK_PRESENT_MODE_MAILBOX_KHR) {
            context->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

            free(present_modes);
            return true;
        }
    }

    free(present_modes);
    return false;
}

// Sets correct swapchain format on success. In the case of failing to find the
// preferred swapchain, the format is left untouched.
bool try_preferred_swapchain_format(RenderContext* context) {
    SwapChainDescriptor *chain = &context->swapchain;

    for (u32 idx = 0; idx < chain->format_count; idx++) {
        VkSurfaceFormatKHR surface_format = chain->formats[idx];

        if (chain->formats[idx].format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            context->surface_format = surface_format;
            return true;
        }
    }

    return false;
}

/// Find a queue family that supports graphics.
bool find_queue_families(RenderContext *context) {
    u32 count;
    vkGetPhysicalDeviceQueueFamilyProperties(context->device, &count, NULL);

    VkQueueFamilyProperties* families = malloc(count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(context->device, &count, families);

    for (u32 idx = 0; idx < count; idx++) {
        if (families[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context->queue_family_indices = idx;

            free(families);
            return true;
        }
    }

    free(families);
    return false;
}

/// Store the names of all available layers.
void vulkan_valididation_create(ValidationLayers *valid) {
    vkEnumerateInstanceLayerProperties(&valid->layer_count, NULL);

    valid->data = malloc(valid->layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&valid->layer_count, valid->data);

    valid->layers = malloc(valid->layer_count * sizeof(char*));
    for (u32 idx = 0; idx < valid->layer_count; idx++)
        valid->layers[idx] = valid->data[idx].layerName;

    trace_array(valid->layers, valid->layer_count, "layers enabled: ");
}

void vulkan_valididation_destroy(ValidationLayers *valid) {
    free(valid->layers);
    free(valid->data);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_handler(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT &&
        get_log_level() >= LOG_TRACE) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
        get_log_level() >= LOG_INFO) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT &&
        get_log_level() >= LOG_WARN) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT &&
        get_log_level() >= LOG_ERROR) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT) {
        printf("[vulkan] %s\n", callback_data->pMessage);
        exit(1);
    }

    return VK_FALSE;
}

static PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
static PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;

bool vulkan_debugger_create(RenderContext *context) {
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
        .pfnUserCallback = vulkan_debug_handler,
        .pUserData = NULL,
    };

    // find function loaded by `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
    CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)(
        vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT")
    );

    // find function loaded by `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
    DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)(
        vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT")
    );

    if (CreateDebugUtilsMessengerEXT == NULL || DestroyDebugUtilsMessengerEXT == NULL)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    return CreateDebugUtilsMessengerEXT(
        context->instance,
        &create_info,
        NULL,
        &context->messenger
    ) == VK_SUCCESS;
}

void vulkan_debugger_destroy(RenderContext *context) {
    if (get_log_level() < LOG_INFO)
        return;

    DestroyDebugUtilsMessengerEXT(context->instance, context->messenger, NULL);
}

/// Create a valid swapchain present extent.
void create_swapchain_present_extent(RenderContext *context) {
    VkSurfaceCapabilitiesKHR *capabilities = &context->swapchain.capabilities;

    // some window managers set currentExtent.width to u32::MAX for some reason
    // so we'll just make up a good resolution in this case
    if (capabilities->currentExtent.width == 0xFFFFFFFF) {
        i32 width, height;
        SDL_Vulkan_GetDrawableSize(
            context->window,
            &width,
            &height
        );

        context->dimensions.width = clamp(
            (u32)width,
            capabilities->minImageExtent.width,
            capabilities->maxImageExtent.width
        );

        context->dimensions.height = clamp(
            (u32)height,
            capabilities->minImageExtent.height,
            capabilities->maxImageExtent.height
        );

        return;
    }

    context->dimensions = capabilities->currentExtent;
}

/// Try to create a swapchain with at least one format.
bool vulkan_swapchain_create(RenderContext *context) {
    SwapChainDescriptor* chain = &context->swapchain;
    VkSurfaceCapabilitiesKHR* capabilities = &chain->capabilities;

    VkResult surface_capabilities_fail = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        context->device,
        context->surface,
        capabilities
    );

    if (surface_capabilities_fail)
        return false;

    VkResult surface_formats_fail;
    surface_formats_fail = vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device,
        context->surface,
        &chain->format_count,
        NULL
    );

    if (chain->format_count == 0 || surface_formats_fail)
        return false;

    chain->formats = malloc(chain->format_count * sizeof(VkSurfaceFormatKHR));
    surface_formats_fail = vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device,
        context->surface,
        &chain->format_count,
        chain->formats
    );

    if (surface_formats_fail || chain->format_count == 0)
        return false;

    VkSurfaceFormatKHR fallback_surface_format = {
        .format = VK_FORMAT_UNDEFINED,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    if (!try_preferred_swapchain_format(context))
        context->surface_format = fallback_surface_format;

    if (!try_preferred_present_mode(context))
        context->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

    create_swapchain_present_extent(context);

    // number of images to be held in the swapchain
    chain->image_count = capabilities->minImageCount + 2;

    // NOTE: may want to use `VK_IMAGE_USAGE_TRANSFER_DST_BIT` for image usage
    // as it allows rendering to a seperate image first to perform
    // post-processing

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = chain->image_count,
        .imageFormat = context->surface_format.format,
        .imageExtent = context->dimensions,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = capabilities->currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = context->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    // NOTE: `oldSwapchain` can specify a backup swapchain for when the window
    // get's resized or if the chain becomes invalid

    // VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue
    // families without explicit ownership transfers
    //
    // VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a
    // time and ownership must be explicitly transferred before using it in
    // another queue family. This option offers the best performance

    if (context->present_queue != context->queue) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = &context->queue_family_indices;
    }

    VkResult swapchain_fail;
    swapchain_fail = vkCreateSwapchainKHR(
        context->driver,
        &create_info,
        NULL,
        &chain->data
    );

    if (swapchain_fail)
        return false;

    swapchain_fail = vkGetSwapchainImagesKHR(
        context->driver,
        chain->data,
        &chain->format_count,
        NULL
    );

    if (swapchain_fail) {
        error("failed to read count of swapchain images");
        return false;
    }

    chain->images = malloc(chain->image_count * sizeof(VkImage));
    swapchain_fail = vkGetSwapchainImagesKHR(
        context->driver,
        chain->data,
        &chain->format_count,
        chain->images
    );

    if (swapchain_fail) {
        error("failed to get swapchain images");
        return false;
    }

    return true;
}

void vulkan_swapchain_destroy(RenderContext *context) {
    SwapChainDescriptor* chain = &context->swapchain;

    for (u32 idx = 0; idx < chain->image_count; idx++)
        vkDestroyImageView(context->driver, chain->views[idx], NULL);

    vkDestroySwapchainKHR(context->driver, chain->data, NULL);

    free(chain->formats);
    free(chain->views);
}

/// Try to create a device, associated queue, present queue and surface.
bool vulkan_device_create(RenderContext *context) {
    // find a simple queue that can handle at least graphics for now
    if (!find_queue_families(context)) {
        warn("couldn't find any queue families");
        return false;
    }

    // NOTE: can create multiple logical devices with different requirements
    // for the same physical device

    // a priority for scheduling command buffers between 0.0 and 1.0
    f32 queue_priority = 1.0;

    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = context->queue_family_indices,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    const char* device_extensions[1] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // enable no device feature
    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_create_info,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };

    if (vkCreateDevice(context->device, &device_create_info, NULL, &context->driver)) {
        warn("failed to create driver");
        return false;
    }

    vkGetDeviceQueue(context->driver, context->queue_family_indices, 0, &context->queue);

    if (!SDL_Vulkan_CreateSurface(context->window, context->instance, &context->surface)) {
        warn("failed to create surface");
        return false;
    }

    // NOTE: for now the present_queue and queue will be the same
    VkBool32 surface_supported = false;
    VkResult surface_support_fail = vkGetPhysicalDeviceSurfaceSupportKHR(
        context->device,
        context->queue_family_indices,
        context->surface,
        &surface_supported
    );

    if (!surface_supported || surface_support_fail) {
        warn("selected queue doesn't support required surface");
        return false;
    }

    return true;
}

/// Try to setup a device that supports the required
/// features, extensions and swapchain.
bool vulkan_most_suitable_device_create(RenderContext *context) {
    u32 device_count;
    vkEnumeratePhysicalDevices(context->instance, &device_count, NULL);

    if (device_count == 0) return false;

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(context->instance, &device_count, devices);

    VkPhysicalDeviceType preferred_device = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // try to find a discrete GPU
    VkPhysicalDeviceProperties properties;
    for (u32 idx = 0; idx < device_count; idx++) {
        context->device = devices[idx];

        vkGetPhysicalDeviceProperties(context->device, &properties);
        if (properties.deviceType != preferred_device)
            continue;

        trace("GPU: %s", properties.deviceName);

        if (!matches_device_requirements(context->device))
            continue;

        if (!vulkan_device_create(context))
            continue;

        if (!vulkan_swapchain_create(context))
            continue;

        free(devices);
        return true;
    }

    // try to find any GPU
    for (u32 idx = 0; idx < device_count; idx++) {
        context->device = devices[idx];

        vkGetPhysicalDeviceProperties(context->device, &properties);
        if (properties.deviceType == preferred_device)
            continue;

        trace("GPU: %s", properties.deviceName);

        if (!matches_device_requirements(context->device))
            continue;

        if (!vulkan_device_create(context))
            continue;

        if (!vulkan_swapchain_create(context))
            continue;

        free(devices);
        return true;
    }

    free(devices);
    return false;
}

bool vulkan_instance_create(RenderContext *context) {
    u32 ext_count = 0;
    const char** extensions = get_required_extensions(context->window, &ext_count);

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

        .pfnUserCallback = vulkan_debug_handler,
    };

    if (get_log_level() == LOG_TRACE) {
        ValidationLayers* valid = &context->validation;
        vulkan_valididation_create(valid);

        // enable all supported layers when for debugging
        instance_create_info.enabledLayerCount = valid->layer_count;
        instance_create_info.ppEnabledLayerNames = valid->layers;

        // attach debugger just for `vkDestroyInstance` and `vkCreateInstance`
        instance_create_info.pNext = &debug_create_info;
    }

    u32 opt_count;
    bool x = true;
    x = vkCreateInstance(&instance_create_info, NULL, &context->instance) == 0;

    free(get_optional_extensions(&opt_count));
    free(extensions);

    return x;
}

bool vulkan_image_views_create(RenderContext *context) {
    SwapChainDescriptor *chain = &context->swapchain;

    chain->views = malloc(chain->image_count * sizeof(VkImageView));

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = context->surface_format.format,
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

    for (u32 idx = 0; idx < chain->image_count; idx++) {
        create_info.image = chain->images[idx];
        if (vkCreateImageView(context->driver, &create_info, NULL, &chain->views[idx]))
            return false;
    }

    return true;
}

VkResult vulkan_shader_module_create(RenderContext *context,
                                     char* binary,
                                     u32 binary_size,
                                     VkShaderModule *module) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary_size,
        .pCode = (u32*)binary
    };

    return vkCreateShaderModule(context->driver, &create_info, NULL, module);
}

bool vulkan_render_pass_create(RenderContext *context) {
    VkAttachmentDescription color_attachment = {
        .format = context->surface_format.format,
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
        context->driver,
        &render_pass_info,
        NULL,
        &context->render_pass
    ) == VK_SUCCESS;
}

bool vulkan_pipeline_create(RenderContext *context) {
    u32 vert_size, frag_size;

    char* vert_bin = read_binary("./target/shaders/shader.vert.spv", &vert_size);
    char* frag_bin = read_binary("./target/shaders/shader.frag.spv", &frag_size);

    if (vert_bin == NULL || frag_bin == NULL) {
        error("failed to read shader source");
        return false;
    }

    VkResult r1 = vulkan_shader_module_create(
        context,
        frag_bin,
        frag_size,
        &context->frag
    );

    VkResult r2 = vulkan_shader_module_create(
        context,
        vert_bin,
        vert_size,
        &context->vert
    );

    free(vert_bin);
    free(frag_bin);

    if (r1 || r2) {
        error("failed to create shader module");
        return false;
    }

    VkPipelineShaderStageCreateInfo vert_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = context->frag,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = context->vert,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        vert_shader_create_info,
        frag_shader_create_info
    };

    context->dynamic_states = malloc(2 * sizeof(VkDynamicState));
    context->dynamic_state_count = 2;

    context->dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    context->dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamic_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates = context->dynamic_states,
        .dynamicStateCount = context->dynamic_state_count
    };

    // TODO: `pVertexBindingDescriptions` and `pVertexAttributeDescriptions`
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
    };

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

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    context->viewport.x = 0.0;
    context->viewport.y = 0.0;
    context->viewport.width = (float)context->dimensions.width;
    context->viewport.height = (float)context->dimensions.height;
    context->viewport.minDepth = 0.0;
    context->viewport.maxDepth = 1.0;

    context->scissor.offset.x = 0;
    context->scissor.offset.y = 0;
    context->scissor.extent = context->dimensions;

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pViewports = &context->viewport,
        .viewportCount = 1,
        .pScissors = &context->scissor,
        .scissorCount = 1
    };

    // `polygonMode` can be used with `VK_POLYGON_MODE_LINE` for wireframe
    //
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

    if (vkCreatePipelineLayout(context->driver, &pipeline_layout_info, NULL, &context->pipeline_layout)) {
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .layout = context->pipeline_layout,
        .renderPass = context->render_pass,
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

    // NOTE: `vkCreateGraphicsPipelines` takes a list of pipelines to create at once
    return vkCreateGraphicsPipelines(
        context->driver,
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        NULL,
        &context->pipeline
    ) == VK_SUCCESS;
}

void vulkan_pipeline_destroy(RenderContext *context) {
    vkDestroyPipeline(context->driver, context->pipeline, NULL);
    vkDestroyPipelineLayout(context->driver, context->pipeline_layout, NULL);
    vkDestroyShaderModule(context->driver, context->vert, NULL);
    vkDestroyShaderModule(context->driver, context->frag, NULL);

    free(context->dynamic_states);
}

bool vulkan_framebuffers_create(RenderContext *context) {
    SwapChainDescriptor *chain = &context->swapchain;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = context->render_pass,
        .attachmentCount = 1,
        .width = context->dimensions.width,
        .height = context->dimensions.height,
        .layers = 1
    };

    chain->framebuffers = malloc(chain->image_count * sizeof(VkFramebuffer));

    for (u32 idx = 0; idx < chain->image_count; idx++) {
        VkImageView attachments[1] = { chain->views[idx] };
        framebuffer_info.pAttachments = attachments;

        VkResult framebuffer_fail = vkCreateFramebuffer(
            context->driver,
            &framebuffer_info,
            NULL,
            &chain->framebuffers[idx]
        );

        if (framebuffer_fail)
            return false;
    }

    return true;
}

void vulkan_framebuffers_destroy(VkDevice driver, SwapChainDescriptor *chain) {
    for (u32 idx = 0; idx < chain->image_count; idx++)
        vkDestroyFramebuffer(driver, chain->framebuffers[idx], NULL);

    free(chain->framebuffers);
}

bool vulkan_cmd_pool_create(RenderContext *context) {
    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context->queue_family_indices
    };

    return vkCreateCommandPool(
        context->driver,
        &create_info,
        NULL,
        &context->cmd_pool
    ) == VK_SUCCESS;
}

bool vulkan_cmd_buffer_alloc(RenderContext *context) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->cmd_pool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    return vkAllocateCommandBuffers(
        context->driver,
        &alloc_info,
        &context->cmd_buffer
    ) == VK_SUCCESS;
}

bool vulkan_sync_primitives_create(RenderContext *context) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkResult r1 = vkCreateSemaphore(
        context->driver,
        &semaphore_info,
        NULL,
        &context->image_available
    );

    VkResult r2 = vkCreateSemaphore(
        context->driver,
        &semaphore_info,
        NULL,
        &context->render_finished
    );

    VkResult r3 = vkCreateFence(
        context->driver,
        &fence_info,
        NULL,
        &context->in_flight_fence
    );

    return (r1 & r2 & r3) == VK_SUCCESS;
}

void vulkan_sync_primitives_destroy(RenderContext *context) {
    vkDestroySemaphore(context->driver, context->image_available, NULL);
    vkDestroySemaphore(context->driver, context->render_finished, NULL);
    vkDestroyFence(context->driver, context->in_flight_fence, NULL);
}

bool vulkan_record_cmd_buffer(RenderContext *context, u32 image_idx) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(context->cmd_buffer, &begin_info))
        return false;

    /* ------------------------ render pass ------------------------ */

    // clear the screen with black 100% opacity
    VkClearValue clear_color = {{{0.0, 0.0, 0.0, 0.0}}};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context->render_pass,
        .framebuffer = context->swapchain.framebuffers[image_idx],
        .renderArea.offset = {0, 0},
        .renderArea.extent = context->dimensions,
        .pClearValues = &clear_color,
        .clearValueCount = 1,
    };

    vkCmdBeginRenderPass(
        context->cmd_buffer,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

    vkCmdBindPipeline(
        context->cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        context->pipeline
    );

    // setting necessary dynamic state
    vkCmdSetViewport(context->cmd_buffer, 0, 1, &context->viewport);
    vkCmdSetScissor(context->cmd_buffer, 0, 1, &context->scissor);

    vkCmdDraw(context->cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(context->cmd_buffer);

    /* ------------------------------------------------------------- */

    return vkEndCommandBuffer(context->cmd_buffer) == VK_SUCCESS;
}

void vulkan_engine_create(RenderContext *context) {
    if (!vulkan_instance_create(context))
        panic("failed to create instance");

    if (!vulkan_debugger_create(context))
        panic("failed to attach debugger");

    if (!vulkan_most_suitable_device_create(context))
        panic("failed to setup any GPU");

    if (!vulkan_image_views_create(context))
        panic("failed to create image views");

    if (!vulkan_render_pass_create(context))
        panic("failed to create render pass");

    if (!vulkan_pipeline_create(context))
        panic("failed to create a pipeline");

    if (!vulkan_framebuffers_create(context))
        panic("failed to create framebuffer");

    if (!vulkan_cmd_pool_create(context))
        panic("failed to create command pool");

    if (!vulkan_cmd_buffer_alloc(context))
        panic("failed to create command buffer");

    if (!vulkan_sync_primitives_create(context))
        panic("failed to create synchronization primitives");

    info("vulkan engine created");
}

// FIXME: layers appear to be unloading twice
void vulkan_engine_destroy(RenderContext *context) {
    vkDeviceWaitIdle(context->driver);

    vulkan_sync_primitives_destroy(context);
    vkDestroyCommandPool(context->driver, context->cmd_pool, NULL);
    vulkan_framebuffers_destroy(context->driver, &context->swapchain);
    vulkan_pipeline_destroy(context);
    vkDestroyRenderPass(context->driver, context->render_pass, NULL);
    vulkan_swapchain_destroy(context);
    vkDestroyDevice(context->driver, NULL);
    vulkan_debugger_destroy(context);
    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    vkDestroyInstance(context->instance, NULL);

    if (get_log_level() == LOG_TRACE) {
        vulkan_valididation_destroy(&context->validation);
    }

    info("vulkan engine destroyed");
}

// 1. Wait for the previous frame to finish
// 2. Acquire an image from the swap chain
// 3. Record a command buffer which draws the scene onto that image
// 4. Submit the recorded command buffer
// 5. Present the swap chain image

void vulkan_engine_render(RenderContext* context) {
    vkWaitForFences(context->driver, 1, &context->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context->driver, 1, &context->in_flight_fence);

    u32 image_idx;
    VkResult acquire_fail = vkAcquireNextImageKHR(
        context->driver,
        context->swapchain.data,
        UINT64_MAX,
        context->image_available,
        VK_NULL_HANDLE,
        &image_idx
    );

    if (acquire_fail) {
        warn("failed to acquire next image in swapchain");
        return;
    }

    vkResetCommandBuffer(context->cmd_buffer, 0);
    vulkan_record_cmd_buffer(context, image_idx);

    VkSemaphore wait_semaphores[1] = { context->image_available };
    VkPipelineStageFlags wait_stages[1] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    // NOTE: each entry in `wait_stages` corresponds to a semaphore in `wait_semaphores`
    VkSemaphore signal_semaphores[1] = { context->render_finished };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pWaitSemaphores = wait_semaphores,
        .waitSemaphoreCount = 1,
        .pWaitDstStageMask = wait_stages,
        .pCommandBuffers = &context->cmd_buffer,
        .commandBufferCount = 1,
        .pSignalSemaphores = signal_semaphores,
        .signalSemaphoreCount = 1
    };

    if (vkQueueSubmit(context->queue, 1, &submit_info, context->in_flight_fence)) {
        warn("failed to submit queue tasks");
        return;
    }

    VkSwapchainKHR swapchains[1] = { context->swapchain.data };

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pWaitSemaphores = signal_semaphores,
        .waitSemaphoreCount = 1,
        .pSwapchains = swapchains,
        .swapchainCount = 1,
        .pImageIndices = &image_idx
    };

    if (vkQueuePresentKHR(context->queue, &present_info))
        return;
}
