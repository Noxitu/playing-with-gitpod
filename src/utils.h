#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <climits>
#include <functional>
#include <vector>

namespace noxitu::vulkan
{
    constexpr static const uint64_t INFINITE_TIMEOUT = std::numeric_limits<uint64_t>::max();

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
}
