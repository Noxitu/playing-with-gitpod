#pragma once
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <functional>
#include <iomanip>
#include <vector>

namespace noxitu::logger
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

namespace noxitu
{
    template<typename ...Args>
    logger::LogHeader log(const char *source, int line=-1)
    {
        using namespace std::chrono;
        using namespace noxitu::logger;

        const int64_t timestamp = duration_cast<milliseconds>(Clock::now()-ZERO_TIMESTAMP).count();

        return {timestamp, source, line};
    }

    template<typename Type>
    class span
    {
    private:
        Type *m_ptr;
        size_t m_size;

    public:
        span(Type *ptr, size_t size) : m_ptr(ptr), m_size(size) {}

        Type &operator[](size_t index) { return m_ptr[index]; }
        const Type &operator[](size_t index) const { return m_ptr[index]; }

        Type* begin() { return m_ptr; }
        Type* end() { return m_ptr+m_size; }

        const Type* begin() const { return m_ptr; }
        const Type* end() const { return m_ptr+m_size; }

        operator span<const Type>() const { return span<const Type>(m_ptr, m_size); }
    };
}

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

    int findQueueFamilyIndex(vk::PhysicalDevice physicalDevice,
                             std::function<bool(const vk::QueueFamilyProperties&)> condition)
    {
        const std::vector<vk::QueueFamilyProperties> queueFamiliyProperties = physicalDevice.getQueueFamilyProperties();

        auto it = std::find_if(queueFamiliyProperties.begin(), queueFamiliyProperties.end(), condition);

        if (it == queueFamiliyProperties.end())
            throw std::runtime_error("No valid queue family");

        return std::distance(queueFamiliyProperties.begin(), it);
    }

    int findMemoryTypeIndex(vk::PhysicalDevice physicalDevice,
                            vk::MemoryRequirements memoryRequirements,
                            vk::MemoryPropertyFlags requiredMemoryPropertyFlags)
    {
        const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

        for (int i = 0; i < static_cast<int>(memoryProperties.memoryTypeCount); ++i)
        {
            const uint32_t currentTypeBit = (1<<i);

            const bool hasAllowedType = ((memoryRequirements.memoryTypeBits & currentTypeBit) != 0);
            const bool hasRequiredProperties = ((memoryProperties.memoryTypes[i].propertyFlags & requiredMemoryPropertyFlags) == requiredMemoryPropertyFlags);

            if (hasAllowedType && hasRequiredProperties)
                return i;
        }

        throw std::runtime_error("Failed to find memory type.");
    }
}
