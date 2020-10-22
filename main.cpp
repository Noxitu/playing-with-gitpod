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

    bool can_enable_validation_layer()
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
            
            const auto instance = vk::createInstance(createInfo, nullptr);

            return instance;
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

        return builder.create();
    }();

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