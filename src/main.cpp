#include "shaders/comp.spv.h"
#include "validation_layer.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace noxitu::vulkan
{
    constexpr static const uint64_t INFINITE_TIMEOUT = std::numeric_limits<uint64_t>::max();

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

    class QueuePriorities
    {
    private:
        std::vector<float> m_priorities;

    public:
        template<typename ...Arg>
        QueuePriorities(Arg ...values)
        {
            m_priorities = {static_cast<float>(values)...};
        }

        operator const float*() const
        {
            return m_priorities.data();
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

        std::tuple<vk::Device, vk::Queue, int> createDevice(const vk::PhysicalDevice &physicalDevice)
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
                1,
                QueuePriorities(1.0)
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

            return {device, queue, queueFamilyIndex};
        }
    };

    vk::Buffer createBuffer(vk::Device device, int bufferSize)
    {
        return device.createBuffer(
            vk::BufferCreateInfo(
                {},
                bufferSize,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::SharingMode::eExclusive
            )
        );
    }

    vk::DeviceMemory allocateBuffer(vk::Buffer buffer, vk::PhysicalDevice physicalDevice, vk::Device device)
    {
        const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
        vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(buffer);

        const int memoryTypeIndex = [&]()
        {
            const auto requiredTypeMask = memoryRequirements.memoryTypeBits;
            const auto requiredProperties = (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            for (int i = 0; i < static_cast<int>(memoryProperties.memoryTypeCount); ++i)
            {
                const uint32_t currentTypeMask = (1<<i);
                const bool hasRequiredType = (currentTypeMask & requiredTypeMask);
                const bool hasRequiredProperties = ((memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties);

                if (hasRequiredType && hasRequiredProperties)
                    return i;
            }

            throw std::runtime_error("Failed to find memory.");
        }();

        vk::DeviceMemory deviceMemory = device.allocateMemory(
            vk::MemoryAllocateInfo(
                memoryRequirements.size,
                memoryTypeIndex
            )
        );

        device.bindBufferMemory(buffer, deviceMemory, 0);

        return deviceMemory;
    }

    std::tuple<vk::DescriptorPool, std::vector<vk::DescriptorSet>, std::vector<vk::DescriptorSetLayout>> 
    createDescriptors(vk::Device device, vk::Buffer buffer, int bufferSize)
    {
        const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindigns = {
            vk::DescriptorSetLayoutBinding(
                0,
                vk::DescriptorType::eStorageBuffer,
                1,
                vk::ShaderStageFlagBits::eCompute
            )
        };

        const std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {
            device.createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo(
                    {},
                    descriptorBindigns.size(),
                    descriptorBindigns.data()
                )
            )
        };

        const std::vector<vk::DescriptorPoolSize> poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
        };

        vk::DescriptorPool descriptorPool = device.createDescriptorPool(
            vk::DescriptorPoolCreateInfo(
                {},
                1,
                poolSizes.size(),
                poolSizes.data()
            )
        );

        std::vector<vk::DescriptorSet> descriptorSets = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo(
                descriptorPool,
                descriptorSetLayouts.size(),
                descriptorSetLayouts.data()
            )
        );

        vk::DescriptorBufferInfo bufferInfo(
              buffer,
              0,
              bufferSize
        );

        const std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {
            vk::WriteDescriptorSet(
                descriptorSets.at(0),
                0,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &bufferInfo,
                nullptr
            )
        };

        device.updateDescriptorSets(
            writeDescriptorSets,
            {}
        );

        return {descriptorPool, descriptorSets, descriptorSetLayouts};
    }

    std::tuple<vk::Pipeline, vk::PipelineLayout, vk::ShaderModule> createPipeline(vk::Device device, const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts)
    {
        vk::ShaderModule shader = device.createShaderModule(
            vk::ShaderModuleCreateInfo(
                {},
                src_shaders_comp_spv_len,
                reinterpret_cast<uint32_t*>(src_shaders_comp_spv)
            )
        );

        vk::PipelineLayout pipelineLayout = device.createPipelineLayout(
            vk::PipelineLayoutCreateInfo(
                {},
                descriptorSetLayouts.size(),
                descriptorSetLayouts.data()
            )
        );

        vk::Pipeline pipeline = device.createComputePipeline(
            {},
            vk::ComputePipelineCreateInfo(
                {},
                vk::PipelineShaderStageCreateInfo(
                    {},
                    vk::ShaderStageFlagBits::eCompute,
                    shader,
                    "main"
                ),
                pipelineLayout
            )
        );

        return {pipeline, pipelineLayout, shader};
    }

    std::tuple<vk::CommandPool, vk::CommandBuffer> createCommandBuffer(vk::Device device,
                             vk::Pipeline pipeline,
                             vk::PipelineLayout pipelineLayout,
                             const std::vector<vk::DescriptorSet> &descriptorSets,
                             int queueFamilyIndex)
    {
        vk::CommandPool commandPool = device.createCommandPool(
            vk::CommandPoolCreateInfo(
                {},
                queueFamilyIndex
            )
        );

        const vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(
            vk::CommandBufferAllocateInfo(
                commandPool,
                vk::CommandBufferLevel::ePrimary,
                1
            )
        ).at(0);

        commandBuffer.begin(
            vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
        );

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            pipelineLayout,
            0,
            descriptorSets,
            {}
        );

        commandBuffer.dispatch(128/32, 128/32, 1);

        commandBuffer.end();

        return {commandPool, commandBuffer};
    }

    std::function<void()> submitCommandBuffer(vk::Device device, const std::vector<vk::CommandBuffer> &commandBuffers, vk::Queue queue)
    {
        vk::Fence fence = device.createFence(vk::FenceCreateInfo());

        queue.submit(
            {
                vk::SubmitInfo(
                    0,
                    nullptr,
                    nullptr,
                    commandBuffers.size(),
                    commandBuffers.data()
                )
            },
            fence
        );

        return [&device, fence=fence]()
        {
            device.waitForFences({fence}, VK_TRUE, INFINITE_TIMEOUT);
            device.destroyFence(fence);
        };
    }
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

int main(const int, const char* const[]) try
{
    noxitu::vulkan::InstanceBuilder builder;
    
    const bool enable_validation_layer = noxitu::vulkan::can_enable_validation_layer();

    if (enable_validation_layer)
        builder.enable_validation_layer();
    else
        std::cerr << "Validation layer is not available!" << std::endl;

    const auto instance = builder.createInstance();

#if __linux__ and 0
    noxitu::vulkan::DebugReportCallback debugCallback(std::ofstream("/tmp/vulkan_log.txt"));
#else
    noxitu::vulkan::DebugReportCallback debugCallback(std::cerr);
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

    const auto [device, queue, queueFamilyIndex] = builder.createDevice(physicalDevice);

    const int bufferSize = 128*128;

    const vk::Buffer buffer = noxitu::vulkan::createBuffer(device, bufferSize);
    const vk::DeviceMemory memory = noxitu::vulkan::allocateBuffer(buffer, physicalDevice, device);

    const auto [descriptorPool, descriptorSets, descriptorSetLayouts] = noxitu::vulkan::createDescriptors(device, buffer, bufferSize);

    const auto [pipeline, pipelineLayout, shaderModule] = noxitu::vulkan::createPipeline(device, descriptorSetLayouts);

    const auto [commandPool, commandBuffer] = noxitu::vulkan::createCommandBuffer(device, pipeline, pipelineLayout, descriptorSets, queueFamilyIndex);

    const auto wait = noxitu::vulkan::submitCommandBuffer(device, {commandBuffer}, queue);
    wait();

    std::cerr << "destroying" << std::endl;

    device.freeCommandBuffers(commandPool, {commandBuffer});
    device.destroyCommandPool(commandPool);

    device.destroyShaderModule(shaderModule);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyPipeline(pipeline);

    for (const auto &descriptorSetLayout : descriptorSetLayouts)
        device.destroyDescriptorSetLayout(descriptorSetLayout);
    device.destroyDescriptorPool(descriptorPool);
    device.destroyBuffer(buffer);
    device.freeMemory(memory);
    device.destroy();
    instance.destroy();

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