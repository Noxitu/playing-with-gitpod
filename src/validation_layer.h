#pragma once
#include <vulkan/vulkan.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace noxitu
{
    namespace logger
    {
        struct LogHeader
        {
            int64_t timestamp;
            const char *source;
            int line;

            friend inline std::ostream& operator<< (std::ostream &out, const LogHeader &header)
            {
                out << '[' << std::setw(6) << header.timestamp << std::setw(0) << "ms]"
                    << '[' << header.source; 
                    
                if (header.line != -1)
                    out << ':' << header.line;
                
                out << "]: ";
                return out;
            }
        };

        using Clock = std::chrono::steady_clock;
        static const Clock::time_point ZERO_TIMESTAMP = Clock::now();
    }

    template<typename ...Args>
    logger::LogHeader log(const char *source, int line=-1)
    {
        using namespace std::chrono;
        using namespace noxitu::logger;

        const int64_t timestamp = duration_cast<milliseconds>(Clock::now()-ZERO_TIMESTAMP).count();

        return {timestamp, source, line};
    }
}

namespace noxitu::vulkan::validation_layer
{
    inline bool canEnableValidationLayer()
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
    inline auto createDebugCallback(const vk::Instance &instance, Callback *callbackPtr, bool verbose = false)
    {
        vk::DebugReportFlagsEXT flags = vk::DebugReportFlagBitsEXT::eError 
                                      | vk::DebugReportFlagBitsEXT::eWarning 
                                      | vk::DebugReportFlagBitsEXT::ePerformanceWarning;

        if (verbose)
        {
            flags |= vk::DebugReportFlagBitsEXT::eInformation
                   | vk::DebugReportFlagBitsEXT::eDebug;
        }

        vk::DebugReportCallbackCreateInfoEXT info(
            flags,
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

    void destroyDebugCallback(vk::Instance instance, vk::DebugReportCallbackEXT callback)
    {
        auto vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance.getProcAddr("vkDestroyDebugReportCallbackEXT"));
        
        if (vkDestroyDebugReportCallbackEXT == nullptr) {
            throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
        }

        vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
    }

    class DebugReportCallback
    {
    private:
        
        std::shared_ptr<std::ostream> m_outputStream;

    public:
        DebugReportCallback(std::ostream &outputStream) :
            m_outputStream(&outputStream, [](auto){})
        {}

        DebugReportCallback(std::ofstream &&outputStream) :
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
            const std::string source = std::string("Vulkan::") + layerPrefix;
            *m_outputStream << log(source.c_str()) << message << std::endl;

            return false;
        }
    };

    class ValidationLayer
    {
    private:
        std::vector<std::shared_ptr<void>> m_reporterCallbacks;
        bool m_enabled = false;
    
    public:
        bool enable(std::vector<const char*> &enabledLayers, std::vector<const char*> &enabledExtensions)
        {
            if (canEnableValidationLayer())
            {
                enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
                enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
                m_enabled = true;
            }

            return m_enabled;
        }

        void addCallback(vk::Instance instance, DebugReportCallback reporterCallback, bool verbose = false)
        {
            if (m_enabled)
            {
                std::shared_ptr<DebugReportCallback> reporterCallbackPtr = std::make_shared<DebugReportCallback>(std::move(reporterCallback));

                const vk::DebugReportCallbackEXT handle = createDebugCallback(instance, reporterCallbackPtr.get(), verbose);

                m_reporterCallbacks.emplace_back(
                    reinterpret_cast<void*>(1), // Just any not null pointer.
                    [handle, instance, ptr=std::move(reporterCallbackPtr)](void*)
                    {
                        destroyDebugCallback(instance, handle);
                    }
                );
            }
        }

        void destroy()
        {
            m_reporterCallbacks.clear();
        }
    };
}
