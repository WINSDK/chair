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

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
        get_log_level() == LOG_TRACE) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ||
        get_log_level() >= LOG_INFO) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
        get_log_level() >= LOG_WARN) {
        printf("[vulkan] %s\n", callback_data->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ||
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

VkResult vulkan_debugger_create(RenderContext *context) {
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
    );
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
    u32 count = capabilities->minImageCount + 1;

    // NOTE: may want to use `VK_IMAGE_USAGE_TRANSFER_DST_BIT` for image usage
    // as it allows rendering to a seperate image first to perform
    // post-processing

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = count,
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

    if (vkCreateSwapchainKHR(context->driver, &create_info, NULL, &chain->data)) {
        warn("failed to create swapchain");
        return false;
    }

    return true;
}

void vulkan_swapchain_destroy(VkDevice driver, SwapChainDescriptor* chain) {
    vkDestroySwapchainKHR(driver, chain->data, NULL);

    free(chain->images);
    free(chain->formats);
    free(chain->views);

    info("vulkan swapchain destroyed");
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
bool create_most_suitable_device(RenderContext *context) {
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

bool vulkan_pipeline_create(RenderContext *context) {
    u32 vert_size, frag_size;

    char* vert_bin = read_binary("./target/shaders/shader.vert.spv", &vert_size);
    char* frag_bin = read_binary("./target/shaders/shader.frag.spv", &frag_size);

    if (vert_bin == NULL || frag_bin == NULL) {
        error("failed to read shader source");
        return false;
    }

    VkShaderModule vert, frag;
    if (vulkan_shader_module_create(context, vert_bin, vert_size, &vert) ||
        vulkan_shader_module_create(context, frag_bin, frag_size, &frag)) {
        error("failed to create shader module");
        return false;
    }

    VkPipelineShaderStageCreateInfo vert_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        vert_shader_create_info,
        frag_shader_create_info
    };

    VkDynamicState *dynamic_states = malloc(2 * sizeof(VkDynamicState));

    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamic_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates = dynamic_states,
        .dynamicStateCount = 2
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

    // where in the framebuffer to render to
    VkViewport viewport = {
        .x = 0.0,
        .y = 0.0,
        .width = (float)context->dimensions.width,
        .height = (float)context->dimensions.height,
        .minDepth = 0.0,
        .maxDepth = 1.0
    };

    // region of the viewport to actually display
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = context->dimensions
    };

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pViewports = &viewport,
        .viewportCount = 1,
        .pScissors = &scissor,
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

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(context->driver, &pipeline_layout_info, NULL, &pipeline_layout))
        return false;

    vkDestroyPipelineLayout(context->driver, pipeline_layout, NULL);

    free(dynamic_states);
    free(vert_bin);
    free(frag_bin);

    vkDestroyShaderModule(context->driver, vert, NULL);
    vkDestroyShaderModule(context->driver, frag, NULL);

    return true;
}

void vulkan_pipeline_destroy() {
}

void vulkan_engine_create(RenderContext *context) {
    SwapChainDescriptor *chain = &context->swapchain;

    if (!vulkan_instance_create(context))
        panic("failed to create instance");

    if (vulkan_debugger_create(context))
        panic("failed to attach debugger");

    if (!create_most_suitable_device(context))
        panic("failed to setup any GPU");

    VkResult swapchain_fail;
    swapchain_fail = vkGetSwapchainImagesKHR(
        context->driver,
        chain->data,
        &chain->format_count,
        NULL
    );

    if (swapchain_fail)
        panic("failed to read count of swapchain images");

    chain->images = malloc(chain->image_count * sizeof(VkImage));
    swapchain_fail = vkGetSwapchainImagesKHR(
        context->driver,
        chain->data,
        &chain->format_count,
        chain->images
    );

    if (swapchain_fail)
        panic("failed to read swapchain images");

    if (!vulkan_image_views_create(context))
        panic("failed to create image views");

    if (!vulkan_pipeline_create(context))
        panic("failed to create a pipeline");

    info("vulkan engine created");
}

// FIXME: layers appear to be unloading twice
void vulkan_engine_destroy(RenderContext *context) {
    vulkan_swapchain_destroy(context->driver, &context->swapchain);
    vkDestroyDevice(context->driver, NULL);
    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    DestroyDebugUtilsMessengerEXT(context->instance, context->messenger, NULL);
    vkDestroyInstance(context->instance, NULL);
    vulkan_valididation_destroy(&context->validation);

    info("vulkan engine destroyed");
}

void vulkan_engine_render(RenderContext* context) {
}
