#include "validation_layer.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace noxitu::vulkan
{
    vk::PhysicalDevice findPhysicalDevice(const std::vector<vk::PhysicalDevice> &physicalDevices,
                                        std::function<bool(const vk::PhysicalDevice&)> condition)
    {
        auto it = std::find_if(physicalDevices.begin(), physicalDevices.end(), condition);

        if (it == physicalDevices.end())
            throw std::runtime_error("No physical device found");

        return *it;
    }

    class QueueFamilyIndexFinder
    {
    private:
        std::vector<vk::QueueFamilyProperties> m_queueFamiliyProperties;

    public:
        QueueFamilyIndexFinder(const vk::PhysicalDevice &physicalDevice) :
            m_queueFamiliyProperties(physicalDevice.getQueueFamilyProperties())
        {}

        int find(const std::function<bool(const vk::QueueFamilyProperties&)> &condition) const
        {
            auto it = std::find_if(m_queueFamiliyProperties.begin(), m_queueFamiliyProperties.end(), condition);

            if (it == m_queueFamiliyProperties.end())
                throw std::runtime_error("No valid queue family");

            return std::distance(m_queueFamiliyProperties.begin(), it);
        }
    };

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
            const int queueFamilyIndex = QueueFamilyIndexFinder(physicalDevice).find(
                [](const vk::QueueFamilyProperties &properties)
                {
                    return properties.queueCount > 0 && (properties.queueFlags & vk::QueueFlagBits::eCompute);
                }
            );

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

void printPhysicalDevices(const std::vector<vk::PhysicalDevice> &physicalDevices)
{
    std::cout << "Found Physical Devices:\n";

    for (const vk::PhysicalDevice &device : physicalDevices)
    {
        const vk::PhysicalDeviceProperties properties = device.getProperties();

        std::cout << " * " << properties.deviceName << '\n';
    }

    std::cout << std::endl;
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
        std::cerr << '[' << std::setw(6) << timestamp << std::setw(0) << "ms]"
                  << '[' << layerPrefix << ']'
                  << ": " << message << std::endl;

        return false;
    }
};

int main(const int, const char* const[]) try
{
    noxitu::vulkan::InstanceBuilder builder;
    
    const bool enable_validation_layer = noxitu::vulkan::can_enable_validation_layer();

    if (enable_validation_layer)
        builder.enable_validation_layer();
    else
        std::cerr << "Validation layer is not available!" << std::endl;

    const auto instance = builder.createInstance();

#if __linux__
    DebugReportCallback debugCallback(std::ofstream("/tmp/vulkan_log.txt"));
#else
    DebugReportCallback debugCallback(std::cerr);
#endif

    if (enable_validation_layer)
        noxitu::vulkan::createDebugCallback(instance, &debugCallback);

    const vk::PhysicalDevice physicalDevice = [&]()
    {
        const auto physicalDevices = instance.enumeratePhysicalDevices();
        printPhysicalDevices(physicalDevices);
        return noxitu::vulkan::findPhysicalDevice(physicalDevices, [](auto&) {return true;});
    }();

    std::cout << "Using device: " << physicalDevice.getProperties().deviceName << std::endl;

    const auto [device, queue] = builder.createDevice(physicalDevice);

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