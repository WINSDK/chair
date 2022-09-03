#include "chair.h"
#include "render.h"

#include <stdlib.h>
#include <SDL2/SDL_vulkan.h>

// !!! MacOS requires `VK_KHR_PORTABILITY_subset` to be set

void swap_chain_create(SwapChainDescriptor *desc) {
    // adaptive vsync preset
    desc->present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
}

const char** get_required_extensions(SDL_Window *window, u32 *count) {
    SDL_Vulkan_GetInstanceExtensions(window, count, NULL);

    const char **extensions = malloc((*count + 2) * sizeof(char*));
#ifdef __APPLE__
    extensions[*count] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    ++*count;
#endif

    extensions[*count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    ++*count;

    for (u32 idx = 0; idx < *count; idx++)
        trace("extension enabled: %s", extensions[idx]);

    SDL_Vulkan_GetInstanceExtensions(window, count, extensions);

    return extensions;
}

const char** get_optional_extensions(u32 *count) {
    vkEnumerateInstanceExtensionProperties(NULL, count, NULL);

    VkExtensionProperties* extensions = malloc(*count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, count, extensions);

    const char** extensions_names = malloc(*count * sizeof(char*));

    for (u32 idx = 0; idx < *count; idx++) {
        extensions_names[idx] = extensions[idx].extensionName;
        trace("available extension: %s", extensions[idx].extensionName);
    }

    return extensions_names;
}

const char** get_layers(u32 *count) {
    vkEnumerateInstanceLayerProperties(count, NULL);

    VkLayerProperties* layers = malloc(*count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(count, layers);

    const char** layer_names = malloc(*count * sizeof(char*));
    for (u32 idx = 0; idx < *count; idx++) {
        layer_names[idx] = layers[idx].layerName;
        trace("layer enabled: %s", layers[idx].layerName);
    }

    return layer_names;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_handler(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                trace("vulkan: %s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                info("vulkan: %s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                warn("vulkan: %s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                error("vulkan: %s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
                panic("vulkan: %s", callback_data->pMessage);
                break;
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

bool matches_required_features(VkPhysicalDevice device) {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    const char* feature_lookup[55] = {
        "robustBufferAccess",
        "fullDrawIndexUint32",
        "imageCubeArray",
        "independentBlend",
        "geometryShader",
        "tessellationShader",
        "sampleRateShading",
        "dualSrcBlend",
        "logicOp",
        "multiDrawIndirect",
        "drawIndirectFirstInstance",
        "depthClamp",
        "depthBiasClamp",
        "fillModeNonSolid",
        "depthBounds",
        "wideLines",
        "largePoints",
        "alphaToOne",
        "multiViewport",
        "samplerAnisotropy",
        "textureCompressionETC2",
        "textureCompressionASTC_LDR",
        "textureCompressionBC",
        "occlusionQueryPrecise",
        "pipelineStatisticsQuery",
        "vertexPipelineStoresAndAtomics",
        "fragmentStoresAndAtomics",
        "shaderTessellationAndGeometryPointSize",
        "shaderImageGatherExtended",
        "shaderStorageImageExtendedFormats",
        "shaderStorageImageMultisample",
        "shaderStorageImageReadWithoutFormat",
        "shaderStorageImageWriteWithoutFormat",
        "shaderUniformBufferArrayDynamicIndexing",
        "shaderSampledImageArrayDynamicIndexing",
        "shaderStorageBufferArrayDynamicIndexing",
        "shaderStorageImageArrayDynamicIndexing",
        "shaderClipDistance",
        "shaderCullDistance",
        "shaderFloat64",
        "shaderInt64",
        "shaderInt16",
        "shaderResourceResidency",
        "shaderResourceMinLod",
        "sparseBinding",
        "sparseResidencyBuffer",
        "sparseResidencyImage2D",
        "sparseResidencyImage3D",
        "sparseResidency2Samples",
        "sparseResidency4Samples",
        "sparseResidency8Samples",
        "sparseResidency16Samples",
        "sparseResidencyAliased",
        "variableMultisampleRate",
        "inheritedQueries",
    };

    // enumerate each 32-bit feature flag of `VkPhysicalDeviceFeatures` struct
    for (u32 idx = 0; idx < 55; idx++) {
        u32 supported = ((u32*)(&features))[idx];

        if (supported) trace("feature supported: %s", feature_lookup[idx]);
    }

    return features.geometryShader;
}

bool find_most_suitable_device(RenderContext *context,
                               VkPhysicalDevice *device) {
    u32 device_count;
    vkEnumeratePhysicalDevices(context->instance, &device_count, NULL);

    if (device_count == 0) return false;

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(context->instance, &device_count, devices);

    // try to find a discrete GPU with the required features
    VkPhysicalDeviceProperties properties;
    for (u32 idx = 0; idx < device_count; idx++) {
        vkGetPhysicalDeviceProperties(devices[idx], &properties);

        trace("GPU: %s", properties.deviceName);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            if (matches_required_features(devices[idx])) {
                *device = devices[idx];
                return true;
            }
        }
    }

    // try to find any GPU with the required features
    for (u32 idx = 0; idx < device_count; idx++) {
        if (matches_required_features(devices[idx])) {
            *device = devices[idx];
            return true;
        }
    }

    return false;
}

void vulkan_engine_create(RenderContext *context, SDL_Window *window) {
    u32 ext_count = 0;
    const char** extensions = get_required_extensions(window, &ext_count);

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name,
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info = {
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

    if (get_log_level() >= LOG_INFO) {
        // enable all supported layers when for debugging
        u32 layer_count;
        const char** names = get_layers(&layer_count);

        create_info.enabledLayerCount = layer_count;
        create_info.ppEnabledLayerNames = names;

        // attach debugger just for `vkDestroyInstance` and `vkCreateInstance`
        create_info.pNext = &debug_create_info;
    }

    u32 opt_count;
    const char** opt_extensions = get_optional_extensions(&opt_count);

    if (vkCreateInstance(&create_info, NULL, &context->instance))
        panic("failed to create instance");

    if (vulkan_debugger_create(context))
        panic("failed to attach debugger");

    VkPhysicalDevice device;
    if (!find_most_suitable_device(context, &device))
        panic("failed to find suitable GPU");

    info("vulkan engine created");
}

void vulkan_engine_destroy(RenderContext *context) {
    DestroyDebugUtilsMessengerEXT(context->instance, context->messenger, NULL);
    vkDestroyInstance(context->instance, NULL);

    info("vulkan engine destroyed");
}

void vulkan_engine_render(RenderContext* context) {
}
