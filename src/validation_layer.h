#pragma once
#include <vulkan/vulkan.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
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
                const char* pMessage,
                void *userData
            ) -> VkBool32
            {
                Callback &callback = *reinterpret_cast<Callback*>(userData);
                const bool ret = callback(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
                return ret ? VK_TRUE : VK_FALSE;
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

    class DebugReportCallback
    {
    private:
        using Clock = std::chrono::steady_clock;
        Clock::time_point m_zeroTimestamp;
        std::shared_ptr<std::ostream> m_outputStream;

    public:
        DebugReportCallback(std::ostream &outputStream) :
            m_zeroTimestamp(Clock::now()),
            m_outputStream(&outputStream, [](auto){})
        {}

        DebugReportCallback(std::ofstream &&outputStream) :
            m_zeroTimestamp(Clock::now()),
            m_outputStream(std::make_shared<std::ofstream>(std::move(outputStream)))
        {
        }

        bool operator()(VkDebugReportFlagsEXT,
                        VkDebugReportObjectTypeEXT,
                        uint64_t,
                        size_t,
                        int32_t,
                        const char* layerPrefix,
                        const char* message) const
        {
            const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-m_zeroTimestamp).count();
            *m_outputStream << '[' << std::setw(6) << timestamp << std::setw(0) << "ms]"
                            << '[' << layerPrefix << ']'
                            << ": " << message << std::endl;

            return false;
        }
    };
}
