#include "shaders/comp.spv.h"
#include "utils.h"
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
    vk::Instance createInstance(const vk::ApplicationInfo &applicationInfo, 
                                const std::vector<const char *> &enabledLayers,
                                const std::vector<const char *> &enabledExtensions)
    {
        return vk::createInstance(
            vk::InstanceCreateInfo(
                {},
                &applicationInfo,
                enabledLayers.size(),
                enabledLayers.data(),
                enabledExtensions.size(),
                enabledExtensions.data()
            ),
            nullptr
        );
    }

    std::tuple<vk::Device, vk::Queue, int> createDevice(const vk::PhysicalDevice &physicalDevice,
                                                        const std::vector<const char *> &enabledLayers)
    {
        const int queueFamilyIndex = QueueFamilyIndexFinder(physicalDevice).find(
            [](const vk::QueueFamilyProperties &properties)
            {
                return properties.queueCount > 0 && (properties.queueFlags & vk::QueueFlagBits::eCompute);
            }
        );

        std::vector<vk::DeviceQueueCreateInfo> queueInfos = {
            vk::DeviceQueueCreateInfo(
                {},
                queueFamilyIndex,
                1,
                std::vector<float>{1.0f}.data()
            )
        };

        const vk::Device device = physicalDevice.createDevice(
            vk::DeviceCreateInfo(
                {},
                queueInfos.size(),
                queueInfos.data(),
                enabledLayers.size(),
                enabledLayers.data(),
                0,
                nullptr,
                nullptr
            )
        );

        const vk::Queue queue = device.getQueue(queueFamilyIndex, 0);

        return {device, queue, queueFamilyIndex};
    }

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

        return [device, fence=fence]()
        {
            device.waitForFences({fence}, VK_TRUE, INFINITE_TIMEOUT);
            device.destroyFence(fence);
        };
    }

    template<typename Type>
    std::shared_ptr<span<Type>> mapMemory(vk::Device device, vk::DeviceMemory deviceMemory, int bufferSize)
    {
        void* memory = device.mapMemory(deviceMemory, 0, bufferSize);

        auto spanPtr = std::make_shared<span<Type>>(reinterpret_cast<Type*>(memory), bufferSize/sizeof(Type));

        return std::shared_ptr<span<Type>>(
            spanPtr.get(),
            [device, deviceMemory, spanPtr](auto)
            {
                device.unmapMemory(deviceMemory);
            });
    }
}

void printPhysicalDevices(const std::vector<vk::PhysicalDevice> &physicalDevices)
{
    std::cerr << noxitu::log(__FILE__, __LINE__) << "Found Physical Devices:\n";

    for (const vk::PhysicalDevice &device : physicalDevices)
    {
        const vk::PhysicalDeviceProperties properties = device.getProperties();

        std::cerr << noxitu::log(__FILE__, __LINE__) << " * " << properties.deviceName << '\n';
    }

    std::cerr << noxitu::log(__FILE__, __LINE__) << std::endl;
}

void saveArray(const char *path, const noxitu::vulkan::span<const float> &array)
{
    std::ofstream out(path);
    for (auto value : array)
        out << value << ' ';
}


int main(const int, const char* const[]) try
{
    std::vector<const char*> enabledLayers;
    std::vector<const char*> enabledExtensions;
    
    noxitu::vulkan::validation_layer::ValidationLayer noxituValidationLayer;

    if(!noxituValidationLayer.enable(enabledLayers, enabledExtensions))
    {
        std::cerr << noxitu::log(__FILE__, __LINE__) << "Validation layer is not available!" << std::endl;
    }
        
    vk::ApplicationInfo applicationInfo(
        "Noxitu Application Name",
        0, // App Version
        "Noxitu Engine Name",
        0, // Engine Version
        VK_API_VERSION_1_0
    );

    const vk::Instance instance = noxitu::vulkan::createInstance(applicationInfo, enabledLayers, enabledExtensions);

    noxituValidationLayer.addCallback(instance, std::cerr);
#ifdef __linux__
    noxituValidationLayer.addCallback(instance, std::ofstream("/tmp/vulkan_log.txt"), true);
#endif

    const vk::PhysicalDevice physicalDevice = [&]()
    {
        const auto physicalDevices = instance.enumeratePhysicalDevices();
        printPhysicalDevices(physicalDevices);
        return noxitu::vulkan::findPhysicalDevice(physicalDevices, [](auto&) {return true;});
    }();

    std::cerr << noxitu::log(__FILE__, __LINE__) << "Using device: " << physicalDevice.getProperties().deviceName << std::endl;

    const auto [device, queue, queueFamilyIndex] = noxitu::vulkan::createDevice(physicalDevice, enabledLayers);

    const int bufferSize = 4*sizeof(float)*128*128;

    const vk::Buffer buffer = noxitu::vulkan::createBuffer(device, bufferSize);
    const vk::DeviceMemory memory = noxitu::vulkan::allocateBuffer(buffer, physicalDevice, device);

    const auto [descriptorPool, descriptorSets, descriptorSetLayouts] = noxitu::vulkan::createDescriptors(device, buffer, bufferSize);

    const auto [pipeline, pipelineLayout, shaderModule] = noxitu::vulkan::createPipeline(device, descriptorSetLayouts);

    const auto [commandPool, commandBuffer] = noxitu::vulkan::createCommandBuffer(device, pipeline, pipelineLayout, descriptorSets, queueFamilyIndex);

    const auto wait = noxitu::vulkan::submitCommandBuffer(device, {commandBuffer}, queue);
    wait();

    {
        std::cerr << noxitu::log(__FILE__, __LINE__) << "Saving..." << std::endl;
        const auto memoryView = noxitu::vulkan::mapMemory<float>(device, memory, bufferSize);
        saveArray("/tmp/array.txt", *memoryView);
    }

    std::cerr << noxitu::log(__FILE__, __LINE__) << "Destroying..." << std::endl;

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

    
    noxituValidationLayer.destroy();
    instance.destroy();

    std::cerr << noxitu::log(__FILE__, __LINE__) << "main() done" << std::endl;

    return EXIT_SUCCESS;
}
catch(const std::exception &ex)
{
    std::cerr << noxitu::log(__FILE__, __LINE__) << "main() failed with exception " << typeid(ex).name() << ": " << ex.what() << std::endl;
    return EXIT_FAILURE;
}
catch(const int ex)
{
    std::cerr << noxitu::log(__FILE__, __LINE__) << "main() failed with int " << ex << std::endl;
    return EXIT_FAILURE;
}
catch(const char *ex)
{
    std::cerr << noxitu::log(__FILE__, __LINE__) << "main() failed with const char*: " << ex << std::endl;
    return EXIT_FAILURE;
}
catch(...)
{
    std::cerr << noxitu::log(__FILE__, __LINE__) << "main() failed" << std::endl;
    return EXIT_FAILURE;
}