#include "validation_layer.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace noxitu::vulkan
{
    class InstanceBuilder
    {
    private:
        std::vector<const char*> m_enabledLayers;
        std::vector<const char*> m_enabledExtensions;

    public:
        void enable_validation_layer()
        {
            m_enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
            m_enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        vk::Instance createInstance()
        {
            vk::ApplicationInfo applicationInfo(
                "Noxitu Vulkan App",
                0,
                "noxitu_vulkan_engine",
                0,
                VK_API_VERSION_1_0
            );

            vk::InstanceCreateInfo createInfo(
                {},
                &applicationInfo,
                m_enabledLayers.size(),
                m_enabledLayers.data(),
                m_enabledExtensions.size(),
                m_enabledExtensions.data()
            );
            
            return vk::createInstance(createInfo, nullptr);
        }

        std::pair<vk::Device, vk::Queue> createDevice(const vk::PhysicalDevice &physicalDevice)
        {
            const int queueFamilyIndex = [&]()
            {
                const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

                for (int i = 0; i < static_cast<int>(queueFamilyProperties.size()); ++i)
                {
                    const vk::QueueFamilyProperties &properties = queueFamilyProperties.at(i);

                    if (properties.queueCount > 0 && (properties.queueFlags & vk::QueueFlagBits::eCompute))
                        return i;
                }

                throw std::runtime_error("No valid queue family");
            }();

            std::vector<vk::DeviceQueueCreateInfo> queueInfos;

            queueInfos.push_back(vk::DeviceQueueCreateInfo(
                {},
                queueFamilyIndex,
                1
            ));

            const vk::DeviceCreateInfo info(
                {},
                queueInfos.size(),
                queueInfos.data(),
                m_enabledLayers.size(),
                m_enabledLayers.data(),
                0,
                nullptr,
                nullptr
            );

            const vk::Device device = physicalDevice.createDevice(info);
            const vk::Queue queue = device.getQueue(queueFamilyIndex, 0);

            return {device, queue};
        }
    };
}

int main(const int, const char* const[]) try
{
    noxitu::vulkan::InstanceBuilder builder;
    
    const bool enable_validation_layer = false && noxitu::vulkan::can_enable_validation_layer();

    if (enable_validation_layer)
        builder.enable_validation_layer();
    else
        std::cerr << "Validation layer is not available!" << std::endl;

    const auto instance = builder.createInstance();

    auto callback = [&](
        VkDebugReportFlagsEXT,
        VkDebugReportObjectTypeEXT,
        uint64_t,
        size_t,
        int32_t,
        const char* pLayerPrefix,
        const char* pMessage
    ) {
        std::cerr << "Vulkan Debug(layerPrefix=\"" << pLayerPrefix << "\""
                  << "             message=\"" << pMessage << "\")"
                  << std::endl;

        return false;
    };

    if (enable_validation_layer)
        noxitu::vulkan::createDebugCallback(instance, &callback);

    const auto devices = instance.enumeratePhysicalDevices();

    for (const vk::PhysicalDevice &device : devices)
    {
        const vk::PhysicalDeviceProperties properties = device.getProperties();

        std::cout << " * " << properties.deviceName << std::endl;
    }

    const vk::PhysicalDevice selectedDevice = [&]()
    {
        auto it = std::find_if(devices.begin(), devices.end(), [](auto&) {return true;});

        if (it == devices.end())
            throw std::runtime_error("No physical device found");

        return *it;
    }();

    std::cout << "Using device: " << selectedDevice.getProperties().deviceName << std::endl;

    const auto [device, queue] = builder.createDevice(selectedDevice);

    std::cerr << "main() done" << std::endl;
    return EXIT_SUCCESS;
}
catch(const std::exception &ex)
{
    std::cerr << "main() failed with exception " << typeid(ex).name() << ": " << ex.what() << std::endl;
    return EXIT_FAILURE;
}
catch(const int ex)
{
    std::cerr << "main() failed with int " << ex << std::endl;
    return EXIT_FAILURE;
}
catch(const char *ex)
{
    std::cerr << "main() failed with const char*: " << ex << std::endl;
    return EXIT_FAILURE;
}
catch(...)
{
    std::cerr << "main() failed" << std::endl;
    return EXIT_FAILURE;
}