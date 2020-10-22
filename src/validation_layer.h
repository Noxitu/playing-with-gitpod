#pragma once
#include <vulkan/vulkan.hpp>

#include <stdexcept>
#include <string>

namespace noxitu::vulkan
{
    inline bool can_enable_validation_layer()
    {
        const std::string VALIDATION_LAYER_NAME = "VK_LAYER_LUNARG_standard_validation";

        const auto available_layers = vk::enumerateInstanceLayerProperties();

        const bool is_layer_available = std::any_of(
            available_layers.begin(), 
            available_layers.end(), 
            [&](const auto &layer) { return layer.layerName == VALIDATION_LAYER_NAME; }
        );

        if (!is_layer_available)
            return false;

        const auto available_extensions = vk::enumerateInstanceExtensionProperties();

        const bool is_extension_available = std::any_of(
            available_extensions.begin(), 
            available_extensions.end(), 
            [](const auto &extension) { return extension.extensionName == std::string(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); }
        );

        return is_extension_available;
    }

    template<typename Callback>
    inline auto createDebugCallback(const vk::Instance &instance, Callback *callbackPtr)
    {
        vk::DebugReportCallbackCreateInfoEXT info(
            vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | 
                vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::eDebug,
            +[](
                VkDebugReportFlagsEXT flags,
                VkDebugReportObjectTypeEXT objectType,
                uint64_t object,
                size_t location,
                int32_t messageCode,
                const char* pLayerPrefix,
                const char* pMessage, void *userData) -> VkBool32
            {
                Callback &callback = *reinterpret_cast<Callback*>(userData);
                callback();
                return {};
            },
            callbackPtr
        );

        auto vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance.getProcAddr("vkCreateDebugReportCallbackEXT"));
        
        if (vkCreateDebugReportCallbackEXT == nullptr) {
            throw std::runtime_error("Could not load vkCreateDebugReportCallbackEXT");
        }

        vk::DebugReportCallbackEXT callback;
        vk::Result result = static_cast<vk::Result>( 
            vkCreateDebugReportCallbackEXT(
                instance, 
                reinterpret_cast<const VkDebugReportCallbackCreateInfoEXT*>(&info),
                nullptr, 
                reinterpret_cast<VkDebugReportCallbackEXT*>(&callback)
            )
        );

        return createResultValue(result, callback, VULKAN_HPP_NAMESPACE_STRING"::Instance::createDebugReportCallbackEXT");
    }
}
