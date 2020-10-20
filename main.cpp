#include <cstdlib>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace noxitu::vulkan
{
    template<typename ObjectType, auto FunctionPtr>
    struct enumerate_objects
    {
        template<typename ...Args>
        std::vector<ObjectType> operator()(Args &&...args) const
        {
            uint32_t count;
            FunctionPtr(std::forward<Args>(args)..., &count, nullptr);

            std::vector<ObjectType> objects(count);

            FunctionPtr(std::forward<Args>(args)..., &count, objects.data());

            return objects;
        }
    };

    constexpr const static enumerate_objects<VkLayerProperties, vkEnumerateInstanceLayerProperties> enumerate_instance_layer_properties;
    constexpr const static enumerate_objects<VkExtensionProperties, vkEnumerateInstanceExtensionProperties> enumerate_instance_extension_properties;

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

        const auto available_extensions = enumerate_instance_extension_properties(nullptr);

        const bool is_extension_available = std::any_of(
            available_extensions.begin(), 
            available_extensions.end(), 
            [](const auto &extension) { return extension.extensionName == std::string(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); }
        );

        return is_extension_available;
    }

    class Instance
    {
    private:
        VkInstance m_instance;


    public:
        Instance(VkInstance instance) : m_instance(instance) {}

        operator VkInstance() const
        {
            return m_instance;
        }

        auto enumerate_phisical_devices() const
        {
            return enumerate_objects<VkPhysicalDevice, vkEnumeratePhysicalDevices>{}(m_instance);
        }
    };

    class InstanceBuilder
    {
    private:
        std::vector<const char*> m_enabled_layers;
        std::vector<const char*> m_enabled_extensions;

        VkApplicationInfo m_application_info = {};
        VkInstanceCreateInfo m_create_info = {};

    public:
        void enable_validation_layer()
        {
            m_enabled_layers.push_back("VK_LAYER_LUNARG_standard_validation");
            m_enabled_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        Instance create()
        {
            m_application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            m_application_info.pApplicationName = "Hello world app";
            m_application_info.applicationVersion = 0;
            m_application_info.pEngineName = "awesomeengine";
            m_application_info.engineVersion = 0;
            m_application_info.apiVersion = VK_API_VERSION_1_0;;
            
            m_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            m_create_info.flags = 0;
            m_create_info.pApplicationInfo = &m_application_info;
            
            m_create_info.enabledLayerCount = m_enabled_layers.size();
            m_create_info.ppEnabledLayerNames = m_enabled_layers.data();
            m_create_info.enabledExtensionCount = m_enabled_extensions.size();
            m_create_info.ppEnabledExtensionNames = m_enabled_extensions.data();

            VkInstance instance;

            VkResult result = vkCreateInstance(
                    &m_create_info,
                    NULL,
                    &instance
            );

            if (result != VK_SUCCESS)
                throw std::runtime_error("vkCreateInstance failed");

            return Instance(instance);
        }
    };
}

int main(const int argc, const char* const argv[]) try
{
    const bool enable_validation_layer = noxitu::vulkan::can_enable_validation_layer();

    const noxitu::vulkan::Instance instance = [&]()
    {
        noxitu::vulkan::InstanceBuilder builder;

        if (enable_validation_layer)
            builder.enable_validation_layer();

        return builder.create();
    }();

    const auto devices = instance.enumerate_phisical_devices();

    for (auto &device : devices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        std::cout << " * " << props.deviceName << std::endl;
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