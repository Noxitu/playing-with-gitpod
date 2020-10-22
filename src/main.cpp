#include "validation_layer.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

        vk::Instance create()
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
    };
}

int main(const int argc, const char* const argv[]) try
{
    const bool enable_validation_layer = false && noxitu::vulkan::can_enable_validation_layer();

    const auto instance = [&]()
    {
        noxitu::vulkan::InstanceBuilder builder;

        if (enable_validation_layer)
            builder.enable_validation_layer();
        else
        {
            std::cerr << "Validation layer is not available!" << std::endl;
        };

        return builder.create();
    }();

    auto callback = [&](auto ...) {};

    if (enable_validation_layer)
        noxitu::vulkan::createDebugCallback(instance, &callback);

    const auto devices = instance.enumeratePhysicalDevices();

    for (auto &device : devices)
    {
        const auto properties = device.getProperties();

        std::cout << " * " << properties.deviceName << std::endl;
    }

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