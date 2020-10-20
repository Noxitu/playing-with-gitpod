#include <cstdlib>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace noxitu::vulkan
{
    std::vector<VkLayerProperties> enumerate_instance_layer_properties()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, NULL);

        std::vector<VkLayerProperties> layerProperties(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

        return layerProperties;
    }

    std::vector<VkExtensionProperties> enumerate_instance_extension_properties()
    {
        uint32_t extensionCount;
            
        vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties.data());

        return extensionProperties;
    }

    bool can_enable_validation_layer()
    {
        const std::string VALIDATION_LAYER_NAME = "VK_LAYER_LUNARG_standard_validation";

        const auto available_layers = enumerate_instance_layer_properties();

        const bool is_layer_available = std::any_of(
            available_layers.begin(), 
            available_layers.end(), 
            [&](const auto &layer) { return layer.layerName == VALIDATION_LAYER_NAME; }
        );

        if (!is_layer_available)
            return false;

        const auto available_extensions = enumerate_instance_extension_properties();

        const bool is_extension_available = std::any_of(
            available_extensions.begin(), 
            available_extensions.end(), 
            [](const auto &extension) { return extension.extensionName == std::string(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); }
        );

        return is_extension_available;
    }
}

int main(const int argc, const char* const argv[]) try
{
    std::vector<const char*> enabled_layers;
    std::vector<const char*> enabled_extensions;

    const bool enable_validation_layer = noxitu::vulkan::can_enable_validation_layer();

    if (enable_validation_layer)
    {
        enabled_layers.push_back("VK_LAYER_LUNARG_standard_validation");
        enabled_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    VkApplicationInfo application_info = {};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "Hello world app";
    application_info.applicationVersion = 0;
    application_info.pEngineName = "awesomeengine";
    application_info.engineVersion = 0;
    application_info.apiVersion = VK_API_VERSION_1_0;;
    
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.flags = 0;
    create_info.pApplicationInfo = &application_info;
    
    // Give our desired layers and extensions to vulkan.
    create_info.enabledLayerCount = enabled_layers.size();
    create_info.ppEnabledLayerNames = enabled_layers.data();
    create_info.enabledExtensionCount = enabled_extensions.size();
    create_info.ppEnabledExtensionNames = enabled_extensions.data();

    VkInstance instance;

    {
        VkResult result = vkCreateInstance(
                &create_info,
                NULL,
                &instance
        );

        if (result != VK_SUCCESS)
            throw std::runtime_error("vkCreateInstance failed");
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