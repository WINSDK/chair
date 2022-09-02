#include "chair.h"
#include "render.h"

#include <stdlib.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

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
                trace("%s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                info("%s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                warn("%s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                error("%s", callback_data->pMessage);
                break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
                panic("%s", callback_data->pMessage);
                break;
    }

    return VK_FALSE;
}

VkResult vulkan_engine_debugger(RenderContext *context) {
    if (get_log_level() < LOG_INFO) return VK_SUCCESS;

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

    PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)(
        vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT")
    );

    if (fn == NULL) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(context->instance, &create_info, NULL, &context->messenger);
}

bool vulkan_engine_create(RenderContext *context, SDL_Window *window) {
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
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif

    // enable all supported layers when debugging 
    if (get_log_level() >= LOG_INFO) {
        u32 layer_count;
        const char** names = get_layers(&layer_count);

        create_info.enabledLayerCount = layer_count;
        create_info.ppEnabledLayerNames = names;
    }

    u32 opt_count;
    const char** opt_extensions = get_optional_extensions(&opt_count);

    if (vkCreateInstance(&create_info, NULL, &context->instance))
        panic("failed to create instance");

    if (vulkan_engine_debugger(context))
        panic("failed to attach debugger");

    info("vulkan engine created");
    return true;
}

bool vulkan_engine_destroy(RenderContext *context) {
    vkDestroyInstance(context->instance, NULL);

    info("vulkan engine destroyed");
    return true;
}

bool vulkan_engine_render(RenderContext* context) {

    return true;
}
