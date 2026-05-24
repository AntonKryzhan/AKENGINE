#include <AK/RHI/VulkanRHI.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace AK
{
    namespace
    {
        constexpr u32 MinFramesInFlight = 2;
        constexpr u32 MaxFramesInFlight = 4;
        constexpr u32 InvalidQueueFamily = 0xFFFFFFFFu;

        using VkFlags = u32;
        using VkBool32 = u32;
        using VkDeviceSize = u64;
        using VkSampleMask = u32;

        struct VkInstance_T;
        struct VkPhysicalDevice_T;
        struct VkDevice_T;
        struct VkQueue_T;
        struct VkCommandBuffer_T;

        using VkInstance = VkInstance_T*;
        using VkPhysicalDevice = VkPhysicalDevice_T*;
        using VkDevice = VkDevice_T*;
        using VkQueue = VkQueue_T*;
        using VkCommandBuffer = VkCommandBuffer_T*;
        using VkSurfaceKHR = u64;
        using VkSwapchainKHR = u64;
        using VkImage = u64;
        using VkImageView = u64;
        using VkCommandPool = u64;
        using VkSemaphore = u64;
        using VkFence = u64;
        using VkDeviceMemory = u64;
        using VkBuffer = u64;
        using VkPipelineCache = u64;
        using VkPipeline = u64;
        using VkRenderPass = u64;
        using VkFramebuffer = u64;
        using VkEvent = u64;
        using VkQueryPool = u64;
        using VkBufferView = u64;
        using VkImageLayout = u32;
        using VkResult = i32;
        using VkStructureType = u32;
        using VkFormat = u32;
        using VkColorSpaceKHR = u32;
        using VkPresentModeKHR = u32;
        using VkImageUsageFlags = VkFlags;
        using VkImageAspectFlags = VkFlags;
        using VkAccessFlags = VkFlags;
        using VkPipelineStageFlags = VkFlags;
        using VkImageViewType = u32;
        using VkSharingMode = u32;
        using VkCommandBufferLevel = u32;
        using VkCommandBufferUsageFlags = VkFlags;
        using VkCommandPoolCreateFlags = VkFlags;
        using VkFenceCreateFlags = VkFlags;
        using VkCompositeAlphaFlagBitsKHR = u32;
        using VkSurfaceTransformFlagBitsKHR = u32;

        constexpr VkResult VK_SUCCESS = 0;
        constexpr VkResult VK_INCOMPLETE = 5;
        constexpr u64 VK_NULL_HANDLE_U64 = 0;
        constexpr u64 VkForever = std::numeric_limits<u64>::max();

        constexpr VkStructureType VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
        constexpr VkStructureType VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
        constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;
        constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;
        constexpr VkStructureType VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
        constexpr VkStructureType VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9;
        constexpr VkStructureType VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8;
        constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;
        constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
        constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
        constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
        constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45;
        constexpr VkStructureType VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR = 1000001000;
        constexpr VkStructureType VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;
        constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000;

        constexpr VkFlags VK_QUEUE_GRAPHICS_BIT = 0x00000001u;
        constexpr VkImageUsageFlags VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002u;
        constexpr VkImageUsageFlags VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010u;
        constexpr VkImageAspectFlags VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;
        constexpr VkAccessFlags VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000u;
        constexpr VkAccessFlags VK_ACCESS_MEMORY_READ_BIT = 0x00008000u;
        constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001u;
        constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000u;
        constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000u;
        constexpr VkCommandPoolCreateFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002u;
        constexpr VkCommandBufferUsageFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001u;
        constexpr VkFenceCreateFlags VK_FENCE_CREATE_SIGNALED_BIT = 0x00000001u;

        constexpr VkFormat VK_FORMAT_UNDEFINED = 0;
        constexpr VkFormat VK_FORMAT_B8G8R8A8_UNORM = 44;
        constexpr VkFormat VK_FORMAT_B8G8R8A8_SRGB = 50;
        constexpr VkColorSpaceKHR VK_COLOR_SPACE_SRGB_NONLINEAR_KHR = 0;
        constexpr VkPresentModeKHR VK_PRESENT_MODE_IMMEDIATE_KHR = 0;
        constexpr VkPresentModeKHR VK_PRESENT_MODE_MAILBOX_KHR = 1;
        constexpr VkPresentModeKHR VK_PRESENT_MODE_FIFO_KHR = 2;
        constexpr VkImageLayout VK_IMAGE_LAYOUT_UNDEFINED = 0;
        constexpr VkImageLayout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7;
        constexpr VkImageLayout VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002;
        constexpr VkImageViewType VK_IMAGE_VIEW_TYPE_2D = 1;
        constexpr VkSharingMode VK_SHARING_MODE_EXCLUSIVE = 0;
        constexpr VkSharingMode VK_SHARING_MODE_CONCURRENT = 1;
        constexpr VkCommandBufferLevel VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0;
        constexpr VkSurfaceTransformFlagBitsKHR VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR = 0x00000001u;
        constexpr VkCompositeAlphaFlagBitsKHR VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001u;

        struct VkExtent2D
        {
            u32 width;
            u32 height;
        };

        struct VkOffset3D
        {
            i32 x;
            i32 y;
            i32 z;
        };

        struct VkExtent3D
        {
            u32 width;
            u32 height;
            u32 depth;
        };

        struct VkApplicationInfo
        {
            VkStructureType sType;
            const void* pNext;
            const char* pApplicationName;
            u32 applicationVersion;
            const char* pEngineName;
            u32 engineVersion;
            u32 apiVersion;
        };

        struct VkInstanceCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            const VkApplicationInfo* pApplicationInfo;
            u32 enabledLayerCount;
            const char* const* ppEnabledLayerNames;
            u32 enabledExtensionCount;
            const char* const* ppEnabledExtensionNames;
        };

        struct VkExtensionProperties
        {
            char extensionName[256];
            u32 specVersion;
        };

        struct VkLayerProperties
        {
            char layerName[256];
            u32 specVersion;
            u32 implementationVersion;
            char description[256];
        };

        struct VkDeviceQueueCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            u32 queueFamilyIndex;
            u32 queueCount;
            const float* pQueuePriorities;
        };

        struct VkDeviceCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            u32 queueCreateInfoCount;
            const VkDeviceQueueCreateInfo* pQueueCreateInfos;
            u32 enabledLayerCount;
            const char* const* ppEnabledLayerNames;
            u32 enabledExtensionCount;
            const char* const* ppEnabledExtensionNames;
            const void* pEnabledFeatures;
        };

        struct VkQueueFamilyProperties
        {
            VkFlags queueFlags;
            u32 queueCount;
            u32 timestampValidBits;
            VkExtent3D minImageTransferGranularity;
        };

        struct VkSurfaceCapabilitiesKHR
        {
            u32 minImageCount;
            u32 maxImageCount;
            VkExtent2D currentExtent;
            VkExtent2D minImageExtent;
            VkExtent2D maxImageExtent;
            u32 maxImageArrayLayers;
            VkFlags supportedTransforms;
            VkSurfaceTransformFlagBitsKHR currentTransform;
            VkFlags supportedCompositeAlpha;
            VkImageUsageFlags supportedUsageFlags;
        };

        struct VkSurfaceFormatKHR
        {
            VkFormat format;
            VkColorSpaceKHR colorSpace;
        };

        struct VkSwapchainCreateInfoKHR
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            VkSurfaceKHR surface;
            u32 minImageCount;
            VkFormat imageFormat;
            VkColorSpaceKHR imageColorSpace;
            VkExtent2D imageExtent;
            u32 imageArrayLayers;
            VkImageUsageFlags imageUsage;
            VkSharingMode imageSharingMode;
            u32 queueFamilyIndexCount;
            const u32* pQueueFamilyIndices;
            VkSurfaceTransformFlagBitsKHR preTransform;
            VkCompositeAlphaFlagBitsKHR compositeAlpha;
            VkPresentModeKHR presentMode;
            VkBool32 clipped;
            VkSwapchainKHR oldSwapchain;
        };

        struct VkComponentMapping
        {
            u32 r;
            u32 g;
            u32 b;
            u32 a;
        };

        struct VkImageSubresourceRange
        {
            VkImageAspectFlags aspectMask;
            u32 baseMipLevel;
            u32 levelCount;
            u32 baseArrayLayer;
            u32 layerCount;
        };

        struct VkImageViewCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            VkImage image;
            VkImageViewType viewType;
            VkFormat format;
            VkComponentMapping components;
            VkImageSubresourceRange subresourceRange;
        };

        struct VkCommandPoolCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkCommandPoolCreateFlags flags;
            u32 queueFamilyIndex;
        };

        struct VkCommandBufferAllocateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkCommandPool commandPool;
            VkCommandBufferLevel level;
            u32 commandBufferCount;
        };

        struct VkCommandBufferBeginInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkCommandBufferUsageFlags flags;
            const void* pInheritanceInfo;
        };

        struct VkSemaphoreCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
        };

        struct VkFenceCreateInfo
        {
            VkStructureType sType;
            const void* pNext;
            VkFenceCreateFlags flags;
        };

        struct VkClearColorValue
        {
            float float32[4];
        };

        struct VkImageMemoryBarrier
        {
            VkStructureType sType;
            const void* pNext;
            VkAccessFlags srcAccessMask;
            VkAccessFlags dstAccessMask;
            VkImageLayout oldLayout;
            VkImageLayout newLayout;
            u32 srcQueueFamilyIndex;
            u32 dstQueueFamilyIndex;
            VkImage image;
            VkImageSubresourceRange subresourceRange;
        };

        struct VkSubmitInfo
        {
            VkStructureType sType;
            const void* pNext;
            u32 waitSemaphoreCount;
            const VkSemaphore* pWaitSemaphores;
            const VkPipelineStageFlags* pWaitDstStageMask;
            u32 commandBufferCount;
            const VkCommandBuffer* pCommandBuffers;
            u32 signalSemaphoreCount;
            const VkSemaphore* pSignalSemaphores;
        };

        struct VkPresentInfoKHR
        {
            VkStructureType sType;
            const void* pNext;
            u32 waitSemaphoreCount;
            const VkSemaphore* pWaitSemaphores;
            u32 swapchainCount;
            const VkSwapchainKHR* pSwapchains;
            const u32* pImageIndices;
            VkResult* pResults;
        };

#if defined(_WIN32)
        struct VkWin32SurfaceCreateInfoKHR
        {
            VkStructureType sType;
            const void* pNext;
            VkFlags flags;
            HINSTANCE hinstance;
            HWND hwnd;
        };
#endif

        using PFN_vkVoidFunction = void (*)();
        using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction (*)(VkInstance, const char*);
        using PFN_vkGetDeviceProcAddr = PFN_vkVoidFunction (*)(VkDevice, const char*);
        using PFN_vkCreateInstance = VkResult (*)(const VkInstanceCreateInfo*, const void*, VkInstance*);
        using PFN_vkDestroyInstance = void (*)(VkInstance, const void*);
        using PFN_vkEnumerateInstanceExtensionProperties = VkResult (*)(const char*, u32*, VkExtensionProperties*);
        using PFN_vkEnumerateInstanceLayerProperties = VkResult (*)(u32*, VkLayerProperties*);
        using PFN_vkEnumeratePhysicalDevices = VkResult (*)(VkInstance, u32*, VkPhysicalDevice*);
        using PFN_vkGetPhysicalDeviceQueueFamilyProperties = void (*)(VkPhysicalDevice, u32*, VkQueueFamilyProperties*);
        using PFN_vkEnumerateDeviceExtensionProperties = VkResult (*)(VkPhysicalDevice, const char*, u32*, VkExtensionProperties*);
        using PFN_vkCreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);
        using PFN_vkDestroyDevice = void (*)(VkDevice, const void*);
        using PFN_vkGetDeviceQueue = void (*)(VkDevice, u32, u32, VkQueue*);
        using PFN_vkDeviceWaitIdle = VkResult (*)(VkDevice);
        using PFN_vkDestroySurfaceKHR = void (*)(VkInstance, VkSurfaceKHR, const void*);
        using PFN_vkGetPhysicalDeviceSurfaceSupportKHR = VkResult (*)(VkPhysicalDevice, u32, VkSurfaceKHR, VkBool32*);
        using PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*);
        using PFN_vkGetPhysicalDeviceSurfaceFormatsKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, u32*, VkSurfaceFormatKHR*);
        using PFN_vkGetPhysicalDeviceSurfacePresentModesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, u32*, VkPresentModeKHR*);
        using PFN_vkCreateSwapchainKHR = VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR*, const void*, VkSwapchainKHR*);
        using PFN_vkDestroySwapchainKHR = void (*)(VkDevice, VkSwapchainKHR, const void*);
        using PFN_vkGetSwapchainImagesKHR = VkResult (*)(VkDevice, VkSwapchainKHR, u32*, VkImage*);
        using PFN_vkCreateImageView = VkResult (*)(VkDevice, const VkImageViewCreateInfo*, const void*, VkImageView*);
        using PFN_vkDestroyImageView = void (*)(VkDevice, VkImageView, const void*);
        using PFN_vkCreateCommandPool = VkResult (*)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
        using PFN_vkDestroyCommandPool = void (*)(VkDevice, VkCommandPool, const void*);
        using PFN_vkAllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
        using PFN_vkCreateSemaphore = VkResult (*)(VkDevice, const VkSemaphoreCreateInfo*, const void*, VkSemaphore*);
        using PFN_vkDestroySemaphore = void (*)(VkDevice, VkSemaphore, const void*);
        using PFN_vkCreateFence = VkResult (*)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
        using PFN_vkDestroyFence = void (*)(VkDevice, VkFence, const void*);
        using PFN_vkWaitForFences = VkResult (*)(VkDevice, u32, const VkFence*, VkBool32, u64);
        using PFN_vkResetFences = VkResult (*)(VkDevice, u32, const VkFence*);
        using PFN_vkAcquireNextImageKHR = VkResult (*)(VkDevice, VkSwapchainKHR, u64, VkSemaphore, VkFence, u32*);
        using PFN_vkBeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
        using PFN_vkEndCommandBuffer = VkResult (*)(VkCommandBuffer);
        using PFN_vkCmdPipelineBarrier = void (*)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkFlags, u32, const void*, u32, const void*, u32, const VkImageMemoryBarrier*);
        using PFN_vkCmdClearColorImage = void (*)(VkCommandBuffer, VkImage, VkImageLayout, const VkClearColorValue*, u32, const VkImageSubresourceRange*);
        using PFN_vkQueueSubmit = VkResult (*)(VkQueue, u32, const VkSubmitInfo*, VkFence);
        using PFN_vkQueuePresentKHR = VkResult (*)(VkQueue, const VkPresentInfoKHR*);
#if defined(_WIN32)
        using PFN_vkCreateWin32SurfaceKHR = VkResult (*)(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
#endif

        struct VulkanRuntimeLoader
        {
#if defined(_WIN32)
            HMODULE library = nullptr;
#else
            void* library = nullptr;
#endif
            PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
            PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
            PFN_vkCreateInstance vkCreateInstance = nullptr;
            PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
            PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = nullptr;
            PFN_vkDestroyInstance vkDestroyInstance = nullptr;
            PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
            PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
            PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = nullptr;
            PFN_vkCreateDevice vkCreateDevice = nullptr;
            PFN_vkDestroyDevice vkDestroyDevice = nullptr;
            PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
            PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
            PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
            PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
            PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
            PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
            PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
            PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
            PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
            PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
            PFN_vkCreateImageView vkCreateImageView = nullptr;
            PFN_vkDestroyImageView vkDestroyImageView = nullptr;
            PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
            PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
            PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
            PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
            PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
            PFN_vkCreateFence vkCreateFence = nullptr;
            PFN_vkDestroyFence vkDestroyFence = nullptr;
            PFN_vkWaitForFences vkWaitForFences = nullptr;
            PFN_vkResetFences vkResetFences = nullptr;
            PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
            PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
            PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
            PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
            PFN_vkCmdClearColorImage vkCmdClearColorImage = nullptr;
            PFN_vkQueueSubmit vkQueueSubmit = nullptr;
            PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
#if defined(_WIN32)
            PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
#endif
        };

        struct VulkanRuntimeObjects
        {
            VkInstance instance = nullptr;
            VkSurfaceKHR surface = VK_NULL_HANDLE_U64;
            VkPhysicalDevice physicalDevice = nullptr;
            VkDevice device = nullptr;
            VkQueue graphicsQueue = nullptr;
            VkQueue presentQueue = nullptr;
            VkSwapchainKHR swapchain = VK_NULL_HANDLE_U64;
            std::vector<VkImage> swapchainImages;
            std::vector<VkImageView> imageViews;
            VkCommandPool commandPool = VK_NULL_HANDLE_U64;
            VkCommandBuffer commandBuffer = nullptr;
            VkSemaphore imageAvailable = VK_NULL_HANDLE_U64;
            VkSemaphore renderFinished = VK_NULL_HANDLE_U64;
            VkFence inFlightFence = VK_NULL_HANDLE_U64;
        };

        u32 MakeVkVersion(u32 major, u32 minor, u32 patch)
        {
            return (major << 22u) | (minor << 12u) | patch;
        }

        std::string SelectLoaderLibraryName()
        {
#if defined(_WIN32)
            return "vulkan-1.dll";
#elif defined(__APPLE__)
            return "libvulkan.1.dylib";
#else
            return "libvulkan.so.1";
#endif
        }

        VulkanWindowPlatform DetectDefaultWindowPlatform()
        {
#if defined(_WIN32)
            return VulkanWindowPlatform::Win32;
#elif defined(__APPLE__)
            return VulkanWindowPlatform::Metal;
#else
            return VulkanWindowPlatform::Xlib;
#endif
        }

        void AddUnique(std::vector<std::string>& values, std::string value)
        {
            if (std::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(std::move(value));
            }
        }

        bool ContainsName(const std::vector<std::string>& values, std::string_view name)
        {
            return std::find(values.begin(), values.end(), name) != values.end();
        }

        void AddSurfaceExtensions(VulkanRhiPlan& plan)
        {
            if (plan.headless || !plan.config.requireSwapchain)
            {
                return;
            }

            AddUnique(plan.instanceExtensions, "VK_KHR_surface");

            switch (plan.config.window.platform)
            {
                case VulkanWindowPlatform::Win32:
                    AddUnique(plan.instanceExtensions, "VK_KHR_win32_surface");
                    break;
                case VulkanWindowPlatform::Xlib:
                    AddUnique(plan.instanceExtensions, "VK_KHR_xlib_surface");
                    break;
                case VulkanWindowPlatform::Xcb:
                    AddUnique(plan.instanceExtensions, "VK_KHR_xcb_surface");
                    break;
                case VulkanWindowPlatform::Wayland:
                    AddUnique(plan.instanceExtensions, "VK_KHR_wayland_surface");
                    break;
                case VulkanWindowPlatform::Metal:
                    AddUnique(plan.instanceExtensions, "VK_EXT_metal_surface");
                    break;
                default:
                    ++plan.warnings;
                    break;
            }
        }

        void AddCoreExtensions(VulkanRhiPlan& plan)
        {
            if (plan.config.enableDebugUtils)
            {
                AddUnique(plan.instanceExtensions, "VK_EXT_debug_utils");
            }

            if (plan.config.requireSwapchain)
            {
                AddUnique(plan.deviceExtensions, "VK_KHR_swapchain");
            }
            if (plan.config.requireDynamicRendering)
            {
                AddUnique(plan.deviceExtensions, "VK_KHR_dynamic_rendering");
            }
            if (plan.config.requireTimelineSemaphore)
            {
                AddUnique(plan.deviceExtensions, "VK_KHR_timeline_semaphore");
            }
            if (plan.config.requireDescriptorIndexing)
            {
                AddUnique(plan.deviceExtensions, "VK_EXT_descriptor_indexing");
            }
            if (plan.config.requireSynchronization2)
            {
                AddUnique(plan.deviceExtensions, "VK_KHR_synchronization2");
            }
            if (plan.config.requireBufferDeviceAddress)
            {
                AddUnique(plan.deviceExtensions, "VK_KHR_buffer_device_address");
            }

            if (plan.config.enableValidation)
            {
                AddUnique(plan.validationLayers, "VK_LAYER_KHRONOS_validation");
            }
        }

        template <typename T>
        T LoadGlobalProc(VulkanRuntimeLoader& loader, const char* name)
        {
            if (!loader.vkGetInstanceProcAddr)
            {
                return nullptr;
            }
            return reinterpret_cast<T>(loader.vkGetInstanceProcAddr(nullptr, name));
        }

        template <typename T>
        T LoadInstanceProc(VulkanRuntimeLoader& loader, VkInstance instance, const char* name)
        {
            if (!loader.vkGetInstanceProcAddr)
            {
                return nullptr;
            }
            return reinterpret_cast<T>(loader.vkGetInstanceProcAddr(instance, name));
        }

        template <typename T>
        T LoadDeviceProc(VulkanRuntimeLoader& loader, VkDevice device, const char* name)
        {
            if (!loader.vkGetDeviceProcAddr)
            {
                return nullptr;
            }
            return reinterpret_cast<T>(loader.vkGetDeviceProcAddr(device, name));
        }

        bool LoadVulkanLibrary(VulkanRuntimeLoader& loader)
        {
            const std::string libraryName = SelectLoaderLibraryName();
#if defined(_WIN32)
            loader.library = LoadLibraryA(libraryName.c_str());
            if (!loader.library)
            {
                return false;
            }
            loader.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader.library, "vkGetInstanceProcAddr"));
#else
            loader.library = dlopen(libraryName.c_str(), RTLD_LAZY | RTLD_LOCAL);
            if (!loader.library)
            {
                return false;
            }
            loader.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(loader.library, "vkGetInstanceProcAddr"));
#endif
            if (!loader.vkGetInstanceProcAddr)
            {
                return false;
            }

            loader.vkCreateInstance = LoadGlobalProc<PFN_vkCreateInstance>(loader, "vkCreateInstance");
            loader.vkEnumerateInstanceExtensionProperties = LoadGlobalProc<PFN_vkEnumerateInstanceExtensionProperties>(loader, "vkEnumerateInstanceExtensionProperties");
            loader.vkEnumerateInstanceLayerProperties = LoadGlobalProc<PFN_vkEnumerateInstanceLayerProperties>(loader, "vkEnumerateInstanceLayerProperties");
            return loader.vkCreateInstance && loader.vkEnumerateInstanceExtensionProperties && loader.vkEnumerateInstanceLayerProperties;
        }

        void UnloadVulkanLibrary(VulkanRuntimeLoader& loader)
        {
#if defined(_WIN32)
            if (loader.library)
            {
                FreeLibrary(loader.library);
                loader.library = nullptr;
            }
#else
            if (loader.library)
            {
                dlclose(loader.library);
                loader.library = nullptr;
            }
#endif
        }

        std::vector<std::string> EnumerateInstanceExtensions(VulkanRuntimeLoader& loader)
        {
            std::vector<std::string> result;
            if (!loader.vkEnumerateInstanceExtensionProperties)
            {
                return result;
            }

            u32 count = 0;
            if (loader.vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
            {
                return result;
            }

            std::vector<VkExtensionProperties> properties(count);
            VkResult vk = loader.vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data());
            if (vk != VK_SUCCESS && vk != VK_INCOMPLETE)
            {
                return result;
            }

            result.reserve(count);
            for (u32 i = 0; i < count; ++i)
            {
                result.emplace_back(properties[i].extensionName);
            }
            return result;
        }

        std::vector<std::string> EnumerateInstanceLayers(VulkanRuntimeLoader& loader)
        {
            std::vector<std::string> result;
            if (!loader.vkEnumerateInstanceLayerProperties)
            {
                return result;
            }

            u32 count = 0;
            if (loader.vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || count == 0)
            {
                return result;
            }

            std::vector<VkLayerProperties> properties(count);
            VkResult vk = loader.vkEnumerateInstanceLayerProperties(&count, properties.data());
            if (vk != VK_SUCCESS && vk != VK_INCOMPLETE)
            {
                return result;
            }

            result.reserve(count);
            for (u32 i = 0; i < count; ++i)
            {
                result.emplace_back(properties[i].layerName);
            }
            return result;
        }

        std::vector<std::string> EnumerateDeviceExtensions(VulkanRuntimeLoader& loader, VkPhysicalDevice device)
        {
            std::vector<std::string> result;
            if (!loader.vkEnumerateDeviceExtensionProperties || !device)
            {
                return result;
            }

            u32 count = 0;
            if (loader.vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
            {
                return result;
            }

            std::vector<VkExtensionProperties> properties(count);
            VkResult vk = loader.vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data());
            if (vk != VK_SUCCESS && vk != VK_INCOMPLETE)
            {
                return result;
            }

            result.reserve(count);
            for (u32 i = 0; i < count; ++i)
            {
                result.emplace_back(properties[i].extensionName);
            }
            return result;
        }

        std::vector<const char*> ToCStringPointers(const std::vector<std::string>& strings)
        {
            std::vector<const char*> result;
            result.reserve(strings.size());
            for (const std::string& value : strings)
            {
                result.push_back(value.c_str());
            }
            return result;
        }

        void FilterUnavailable(std::vector<std::string>& requested, const std::vector<std::string>& available, u32& warnings, bool required)
        {
            requested.erase(std::remove_if(requested.begin(), requested.end(), [&](const std::string& value) {
                const bool present = ContainsName(available, value);
                if (!present)
                {
                    ++warnings;
                    return !required;
                }
                return false;
            }), requested.end());
        }

        void LoadInstanceFunctions(VulkanRuntimeLoader& loader, VkInstance instance)
        {
            loader.vkGetDeviceProcAddr = LoadInstanceProc<PFN_vkGetDeviceProcAddr>(loader, instance, "vkGetDeviceProcAddr");
            loader.vkDestroyInstance = LoadInstanceProc<PFN_vkDestroyInstance>(loader, instance, "vkDestroyInstance");
            loader.vkEnumeratePhysicalDevices = LoadInstanceProc<PFN_vkEnumeratePhysicalDevices>(loader, instance, "vkEnumeratePhysicalDevices");
            loader.vkGetPhysicalDeviceQueueFamilyProperties = LoadInstanceProc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(loader, instance, "vkGetPhysicalDeviceQueueFamilyProperties");
            loader.vkEnumerateDeviceExtensionProperties = LoadInstanceProc<PFN_vkEnumerateDeviceExtensionProperties>(loader, instance, "vkEnumerateDeviceExtensionProperties");
            loader.vkCreateDevice = LoadInstanceProc<PFN_vkCreateDevice>(loader, instance, "vkCreateDevice");
            loader.vkDestroySurfaceKHR = LoadInstanceProc<PFN_vkDestroySurfaceKHR>(loader, instance, "vkDestroySurfaceKHR");
            loader.vkGetPhysicalDeviceSurfaceSupportKHR = LoadInstanceProc<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(loader, instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
            loader.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = LoadInstanceProc<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(loader, instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
            loader.vkGetPhysicalDeviceSurfaceFormatsKHR = LoadInstanceProc<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(loader, instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
            loader.vkGetPhysicalDeviceSurfacePresentModesKHR = LoadInstanceProc<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(loader, instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
#if defined(_WIN32)
            loader.vkCreateWin32SurfaceKHR = LoadInstanceProc<PFN_vkCreateWin32SurfaceKHR>(loader, instance, "vkCreateWin32SurfaceKHR");
#endif
        }

        void LoadDeviceFunctions(VulkanRuntimeLoader& loader, VkDevice device)
        {
            loader.vkDestroyDevice = LoadDeviceProc<PFN_vkDestroyDevice>(loader, device, "vkDestroyDevice");
            loader.vkGetDeviceQueue = LoadDeviceProc<PFN_vkGetDeviceQueue>(loader, device, "vkGetDeviceQueue");
            loader.vkDeviceWaitIdle = LoadDeviceProc<PFN_vkDeviceWaitIdle>(loader, device, "vkDeviceWaitIdle");
            loader.vkCreateSwapchainKHR = LoadDeviceProc<PFN_vkCreateSwapchainKHR>(loader, device, "vkCreateSwapchainKHR");
            loader.vkDestroySwapchainKHR = LoadDeviceProc<PFN_vkDestroySwapchainKHR>(loader, device, "vkDestroySwapchainKHR");
            loader.vkGetSwapchainImagesKHR = LoadDeviceProc<PFN_vkGetSwapchainImagesKHR>(loader, device, "vkGetSwapchainImagesKHR");
            loader.vkCreateImageView = LoadDeviceProc<PFN_vkCreateImageView>(loader, device, "vkCreateImageView");
            loader.vkDestroyImageView = LoadDeviceProc<PFN_vkDestroyImageView>(loader, device, "vkDestroyImageView");
            loader.vkCreateCommandPool = LoadDeviceProc<PFN_vkCreateCommandPool>(loader, device, "vkCreateCommandPool");
            loader.vkDestroyCommandPool = LoadDeviceProc<PFN_vkDestroyCommandPool>(loader, device, "vkDestroyCommandPool");
            loader.vkAllocateCommandBuffers = LoadDeviceProc<PFN_vkAllocateCommandBuffers>(loader, device, "vkAllocateCommandBuffers");
            loader.vkCreateSemaphore = LoadDeviceProc<PFN_vkCreateSemaphore>(loader, device, "vkCreateSemaphore");
            loader.vkDestroySemaphore = LoadDeviceProc<PFN_vkDestroySemaphore>(loader, device, "vkDestroySemaphore");
            loader.vkCreateFence = LoadDeviceProc<PFN_vkCreateFence>(loader, device, "vkCreateFence");
            loader.vkDestroyFence = LoadDeviceProc<PFN_vkDestroyFence>(loader, device, "vkDestroyFence");
            loader.vkWaitForFences = LoadDeviceProc<PFN_vkWaitForFences>(loader, device, "vkWaitForFences");
            loader.vkResetFences = LoadDeviceProc<PFN_vkResetFences>(loader, device, "vkResetFences");
            loader.vkAcquireNextImageKHR = LoadDeviceProc<PFN_vkAcquireNextImageKHR>(loader, device, "vkAcquireNextImageKHR");
            loader.vkBeginCommandBuffer = LoadDeviceProc<PFN_vkBeginCommandBuffer>(loader, device, "vkBeginCommandBuffer");
            loader.vkEndCommandBuffer = LoadDeviceProc<PFN_vkEndCommandBuffer>(loader, device, "vkEndCommandBuffer");
            loader.vkCmdPipelineBarrier = LoadDeviceProc<PFN_vkCmdPipelineBarrier>(loader, device, "vkCmdPipelineBarrier");
            loader.vkCmdClearColorImage = LoadDeviceProc<PFN_vkCmdClearColorImage>(loader, device, "vkCmdClearColorImage");
            loader.vkQueueSubmit = LoadDeviceProc<PFN_vkQueueSubmit>(loader, device, "vkQueueSubmit");
            loader.vkQueuePresentKHR = LoadDeviceProc<PFN_vkQueuePresentKHR>(loader, device, "vkQueuePresentKHR");
        }

        void CleanupRuntimeObjects(VulkanRuntimeLoader& loader, VulkanRuntimeObjects& objects)
        {
            if (objects.device && loader.vkDeviceWaitIdle)
            {
                loader.vkDeviceWaitIdle(objects.device);
            }
            if (objects.device && objects.inFlightFence && loader.vkDestroyFence)
            {
                loader.vkDestroyFence(objects.device, objects.inFlightFence, nullptr);
            }
            if (objects.device && objects.renderFinished && loader.vkDestroySemaphore)
            {
                loader.vkDestroySemaphore(objects.device, objects.renderFinished, nullptr);
            }
            if (objects.device && objects.imageAvailable && loader.vkDestroySemaphore)
            {
                loader.vkDestroySemaphore(objects.device, objects.imageAvailable, nullptr);
            }
            if (objects.device && objects.commandPool && loader.vkDestroyCommandPool)
            {
                loader.vkDestroyCommandPool(objects.device, objects.commandPool, nullptr);
            }
            if (objects.device && loader.vkDestroyImageView)
            {
                for (VkImageView view : objects.imageViews)
                {
                    if (view)
                    {
                        loader.vkDestroyImageView(objects.device, view, nullptr);
                    }
                }
            }
            if (objects.device && objects.swapchain && loader.vkDestroySwapchainKHR)
            {
                loader.vkDestroySwapchainKHR(objects.device, objects.swapchain, nullptr);
            }
            if (objects.device && loader.vkDestroyDevice)
            {
                loader.vkDestroyDevice(objects.device, nullptr);
            }
            if (objects.instance && objects.surface && loader.vkDestroySurfaceKHR)
            {
                loader.vkDestroySurfaceKHR(objects.instance, objects.surface, nullptr);
            }
            if (objects.instance && loader.vkDestroyInstance)
            {
                loader.vkDestroyInstance(objects.instance, nullptr);
            }
            objects = {};
        }

        bool IsRequiredSurfaceExtension(const VulkanRhiConfig& config, const std::string& extension)
        {
            if (!config.requireSwapchain || !config.window.valid)
            {
                return false;
            }
            if (extension == "VK_KHR_surface")
            {
                return true;
            }
            if (config.window.platform == VulkanWindowPlatform::Win32 && extension == "VK_KHR_win32_surface")
            {
                return true;
            }
            return false;
        }

#if defined(_WIN32)
        VkResult CreatePlatformSurface(VulkanRuntimeLoader& loader, const VulkanRhiConfig& config, VkInstance instance, VkSurfaceKHR* surface)
        {
            if (!loader.vkCreateWin32SurfaceKHR || config.window.platform != VulkanWindowPlatform::Win32 || !config.window.nativeHandle)
            {
                return -1;
            }

            VkWin32SurfaceCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            createInfo.hinstance = config.window.nativeInstance ? reinterpret_cast<HINSTANCE>(config.window.nativeInstance) : GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(config.window.nativeHandle);
            return loader.vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, surface);
        }
#else
        VkResult CreatePlatformSurface(VulkanRuntimeLoader&, const VulkanRhiConfig&, VkInstance, VkSurfaceKHR*)
        {
            return -1;
        }
#endif

        bool FindQueueFamilies(VulkanRuntimeLoader& loader, VkPhysicalDevice device, VkSurfaceKHR surface, bool needPresent, u32& graphicsFamily, u32& presentFamily)
        {
            graphicsFamily = InvalidQueueFamily;
            presentFamily = InvalidQueueFamily;

            u32 familyCount = 0;
            loader.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
            if (familyCount == 0)
            {
                return false;
            }

            std::vector<VkQueueFamilyProperties> families(familyCount);
            loader.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

            for (u32 i = 0; i < familyCount; ++i)
            {
                if (families[i].queueCount > 0 && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    if (graphicsFamily == InvalidQueueFamily)
                    {
                        graphicsFamily = i;
                    }

                    if (needPresent && surface && loader.vkGetPhysicalDeviceSurfaceSupportKHR)
                    {
                        VkBool32 presentSupported = 0;
                        if (loader.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported) == VK_SUCCESS && presentSupported)
                        {
                            presentFamily = i;
                            graphicsFamily = i;
                            return true;
                        }
                    }
                }
            }

            if (needPresent && surface && loader.vkGetPhysicalDeviceSurfaceSupportKHR)
            {
                for (u32 i = 0; i < familyCount; ++i)
                {
                    if (families[i].queueCount == 0)
                    {
                        continue;
                    }
                    VkBool32 presentSupported = 0;
                    if (loader.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported) == VK_SUCCESS && presentSupported)
                    {
                        presentFamily = i;
                        break;
                    }
                }
            }
            else
            {
                presentFamily = graphicsFamily;
            }

            return graphicsFamily != InvalidQueueFamily && (!needPresent || presentFamily != InvalidQueueFamily);
        }

        VkFormat ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
        {
            if (formats.empty())
            {
                return VK_FORMAT_B8G8R8A8_UNORM;
            }
            if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
            {
                return VK_FORMAT_B8G8R8A8_SRGB;
            }
            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format.format;
                }
            }
            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format.format;
                }
            }
            return formats[0].format;
        }

        VkColorSpaceKHR ChooseColorSpace(const std::vector<VkSurfaceFormatKHR>& formats, VkFormat chosen)
        {
            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == chosen)
                {
                    return format.colorSpace;
                }
            }
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }

        VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, VulkanPresentModePolicy policy, VulkanRuntimeResult& result)
        {
            auto hasMode = [&](VkPresentModeKHR mode) {
                return std::find(modes.begin(), modes.end(), mode) != modes.end();
            };

            if (policy == VulkanPresentModePolicy::ImmediateAllowTearing && hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
            {
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
            if (policy == VulkanPresentModePolicy::MailboxPreferLowLatency && hasMode(VK_PRESENT_MODE_MAILBOX_KHR))
            {
                result.usedMailboxPresent = true;
                return VK_PRESENT_MODE_MAILBOX_KHR;
            }

            result.usedFifoFallback = true;
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, const VulkanRhiConfig& config)
        {
            if (caps.currentExtent.width != std::numeric_limits<u32>::max())
            {
                return caps.currentExtent;
            }

            VkExtent2D extent{config.window.width, config.window.height};
            extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
            return extent;
        }

        const char* FormatName(VkFormat format)
        {
            switch (format)
            {
                case VK_FORMAT_B8G8R8A8_SRGB:
                    return "VK_FORMAT_B8G8R8A8_SRGB";
                case VK_FORMAT_B8G8R8A8_UNORM:
                    return "VK_FORMAT_B8G8R8A8_UNORM";
                default:
                    return "VK_FORMAT_UNKNOWN";
            }
        }

        const char* PresentModeName(VkPresentModeKHR presentMode)
        {
            switch (presentMode)
            {
                case VK_PRESENT_MODE_IMMEDIATE_KHR:
                    return "VK_PRESENT_MODE_IMMEDIATE_KHR";
                case VK_PRESENT_MODE_MAILBOX_KHR:
                    return "VK_PRESENT_MODE_MAILBOX_KHR";
                case VK_PRESENT_MODE_FIFO_KHR:
                    return "VK_PRESENT_MODE_FIFO_KHR";
                default:
                    return "VK_PRESENT_MODE_UNKNOWN";
            }
        }

        bool CreateCommandAndSync(VulkanRuntimeLoader& loader, VulkanRuntimeObjects& objects, u32 graphicsQueueFamily, VulkanRuntimeResult& result)
        {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = graphicsQueueFamily;

            VkResult vk = loader.vkCreateCommandPool(objects.device, &poolInfo, nullptr, &objects.commandPool);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkCreateCommandPool failed";
                return false;
            }

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = objects.commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            vk = loader.vkAllocateCommandBuffers(objects.device, &allocInfo, &objects.commandBuffer);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkAllocateCommandBuffers failed";
                return false;
            }
            result.commandResourcesCreated = true;
            result.lastStage = VulkanRuntimeStage::CommandResources;

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            vk = loader.vkCreateSemaphore(objects.device, &semaphoreInfo, nullptr, &objects.imageAvailable);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkCreateSemaphore(imageAvailable) failed";
                return false;
            }
            vk = loader.vkCreateSemaphore(objects.device, &semaphoreInfo, nullptr, &objects.renderFinished);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkCreateSemaphore(renderFinished) failed";
                return false;
            }
            vk = loader.vkCreateFence(objects.device, &fenceInfo, nullptr, &objects.inFlightFence);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkCreateFence failed";
                return false;
            }
            result.syncObjectsCreated = true;
            result.lastStage = VulkanRuntimeStage::SyncObjects;
            return true;
        }

        bool SubmitClearFrame(VulkanRuntimeLoader& loader, VulkanRuntimeObjects& objects, VulkanRuntimeResult& result)
        {
            if (!objects.swapchain || objects.swapchainImages.empty())
            {
                result.message = "swapchain has no images";
                return false;
            }

            VkResult vk = loader.vkWaitForFences(objects.device, 1, &objects.inFlightFence, 1, VkForever);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkWaitForFences failed";
                return false;
            }
            vk = loader.vkResetFences(objects.device, 1, &objects.inFlightFence);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkResetFences failed";
                return false;
            }

            u32 imageIndex = 0;
            vk = loader.vkAcquireNextImageKHR(objects.device, objects.swapchain, VkForever, objects.imageAvailable, VK_NULL_HANDLE_U64, &imageIndex);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkAcquireNextImageKHR failed";
                return false;
            }
            if (imageIndex >= objects.swapchainImages.size())
            {
                result.message = "vkAcquireNextImageKHR returned an invalid image index";
                return false;
            }

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk = loader.vkBeginCommandBuffer(objects.commandBuffer, &beginInfo);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkBeginCommandBuffer failed";
                return false;
            }

            VkImageSubresourceRange colorRange{};
            colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorRange.baseMipLevel = 0;
            colorRange.levelCount = 1;
            colorRange.baseArrayLayer = 0;
            colorRange.layerCount = 1;

            VkImageMemoryBarrier toTransfer{};
            toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransfer.srcAccessMask = 0;
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = InvalidQueueFamily;
            toTransfer.dstQueueFamilyIndex = InvalidQueueFamily;
            toTransfer.image = objects.swapchainImages[imageIndex];
            toTransfer.subresourceRange = colorRange;
            loader.vkCmdPipelineBarrier(objects.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

            VkClearColorValue clear{};
            clear.float32[0] = 0.035f;
            clear.float32[1] = 0.055f;
            clear.float32[2] = 0.085f;
            clear.float32[3] = 1.0f;
            loader.vkCmdClearColorImage(objects.commandBuffer, objects.swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &colorRange);

            VkImageMemoryBarrier toPresent{};
            toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            toPresent.srcQueueFamilyIndex = InvalidQueueFamily;
            toPresent.dstQueueFamilyIndex = InvalidQueueFamily;
            toPresent.image = objects.swapchainImages[imageIndex];
            toPresent.subresourceRange = colorRange;
            loader.vkCmdPipelineBarrier(objects.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);

            vk = loader.vkEndCommandBuffer(objects.commandBuffer);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkEndCommandBuffer failed";
                return false;
            }

            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &objects.imageAvailable;
            submit.pWaitDstStageMask = &waitStage;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &objects.commandBuffer;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &objects.renderFinished;

            vk = loader.vkQueueSubmit(objects.graphicsQueue, 1, &submit, objects.inFlightFence);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkQueueSubmit failed";
                return false;
            }
            result.clearSubmitted = true;
            result.usedTransferClear = true;
            result.lastStage = VulkanRuntimeStage::ClearFrame;

            VkPresentInfoKHR present{};
            present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &objects.renderFinished;
            present.swapchainCount = 1;
            present.pSwapchains = &objects.swapchain;
            present.pImageIndices = &imageIndex;

            vk = loader.vkQueuePresentKHR(objects.presentQueue, &present);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkQueuePresentKHR failed";
                return false;
            }
            result.presented = true;
            result.lastStage = VulkanRuntimeStage::Present;
            return true;
        }
    }

    NativeWindowSurfaceDesc MakeNativeWindowSurfaceDesc(void* nativeHandle, u32 width, u32 height)
    {
        NativeWindowSurfaceDesc desc{};
        desc.platform = DetectDefaultWindowPlatform();
        desc.nativeHandle = nativeHandle;
        desc.width = width;
        desc.height = height;
        desc.valid = nativeHandle != nullptr && width > 0 && height > 0;
        return desc;
    }

    VulkanRhiConfig MakeDefaultVulkanRhiConfig(const std::string& applicationName)
    {
        VulkanRhiConfig config{};
        config.applicationName = applicationName;
        config.applicationVersionMajor = AK_ENGINE_VERSION_MAJOR;
        config.applicationVersionMinor = AK_ENGINE_VERSION_MINOR;
        config.applicationVersionPatch = AK_ENGINE_VERSION_PATCH;
#if defined(NDEBUG)
        config.enableValidation = false;
        config.enableDebugUtils = false;
#endif
        return config;
    }

    VulkanRhiPlan BuildVulkanRhiPlan(const VulkanRhiConfig& config)
    {
        VulkanRhiPlan plan{};
        plan.config = config;
        plan.loaderLibraryName = SelectLoaderLibraryName();
        plan.headless = !config.requireSwapchain || !config.window.valid;

        VulkanRhiConfig& normalized = plan.config;
        normalized.framesInFlight = std::clamp(normalized.framesInFlight, MinFramesInFlight, MaxFramesInFlight);

        AddCoreExtensions(plan);
        AddSurfaceExtensions(plan);

        plan.queues.push_back({true, true, true, !plan.headless, false, "unified graphics/compute/transfer/present"});
        if (config.preferDedicatedComputeQueue)
        {
            plan.queues.push_back({false, true, false, false, true, "dedicated compute preferred"});
        }
        if (config.preferDedicatedTransferQueue)
        {
            plan.queues.push_back({false, false, true, false, true, "dedicated transfer preferred"});
        }

        for (u32 frame = 0; frame < normalized.framesInFlight; ++frame)
        {
            VulkanFrameResourcePlan framePlan{};
            framePlan.frameIndex = frame;
            plan.frames.push_back(framePlan);
        }

        plan.swapchain.width = config.window.width;
        plan.swapchain.height = config.window.height;
        plan.swapchain.imageCount = normalized.framesInFlight;
        plan.swapchain.presentModePolicy = config.presentModePolicy;

        if (!plan.headless)
        {
            if (config.window.width == 0 || config.window.height == 0)
            {
                ++plan.warnings;
            }
            if (config.window.nativeHandle == nullptr)
            {
                ++plan.warnings;
            }
        }

        if (normalized.framesInFlight < MinFramesInFlight || normalized.framesInFlight > MaxFramesInFlight)
        {
            ++plan.warnings;
        }

        plan.valid = plan.backend == RhiBackend::Vulkan
            && !plan.loaderLibraryName.empty()
            && !plan.frames.empty()
            && (!config.requireSwapchain || plan.headless || (plan.swapchain.width > 0 && plan.swapchain.height > 0));

        std::ostringstream out;
        out << "vulkan rhi plan backend=" << ToString(plan.backend)
            << " platform=" << ToString(plan.config.window.platform)
            << " headless=" << (plan.headless ? "true" : "false")
            << " frames=" << plan.frames.size()
            << " instance_ext=" << plan.instanceExtensions.size()
            << " device_ext=" << plan.deviceExtensions.size()
            << " validation=" << (plan.validationLayers.empty() ? "off" : "on")
            << " warnings=" << plan.warnings;
        plan.summary = out.str();
        return plan;
    }

    VulkanLoaderProbe ProbeVulkanLoader()
    {
        VulkanLoaderProbe probe{};
        probe.attempted = true;
        probe.libraryName = SelectLoaderLibraryName();

#if defined(_WIN32)
        HMODULE module = LoadLibraryA(probe.libraryName.c_str());
        if (!module)
        {
            probe.message = "Vulkan loader not found; install Vulkan Runtime/SDK for real device creation";
            return probe;
        }

        void* getProc = reinterpret_cast<void*>(GetProcAddress(module, "vkGetInstanceProcAddr"));
        probe.loaded = true;
        probe.vkGetInstanceProcAddrResolved = getProc != nullptr;
        probe.message = probe.vkGetInstanceProcAddrResolved ? "Vulkan loader available" : "Vulkan loader loaded but vkGetInstanceProcAddr missing";
        FreeLibrary(module);
#else
        void* module = dlopen(probe.libraryName.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!module)
        {
            probe.message = "Vulkan loader not found; install Vulkan Runtime/SDK for real device creation";
            return probe;
        }

        void* getProc = dlsym(module, "vkGetInstanceProcAddr");
        probe.loaded = true;
        probe.vkGetInstanceProcAddrResolved = getProc != nullptr;
        probe.message = probe.vkGetInstanceProcAddrResolved ? "Vulkan loader available" : "Vulkan loader loaded but vkGetInstanceProcAddr missing";
        dlclose(module);
#endif
        return probe;
    }

    VulkanRhiProbe BuildVulkanRhiProbe(const VulkanRhiConfig& config, bool tryLoadSystemVulkan)
    {
        VulkanRhiProbe probe{};
        probe.plan = BuildVulkanRhiPlan(config);
        if (tryLoadSystemVulkan)
        {
            probe.loader = ProbeVulkanLoader();
        }

        probe.renderGraphCompatible = probe.plan.valid
            && probe.plan.config.requireDynamicRendering
            && probe.plan.config.requireSynchronization2;
        probe.policyCompatible = probe.plan.config.framesInFlight >= MinFramesInFlight
            && probe.plan.config.framesInFlight <= MaxFramesInFlight
            && probe.plan.config.requireTimelineSemaphore;
        probe.swapchainCompatible = probe.plan.headless || (probe.plan.swapchain.width > 0 && probe.plan.swapchain.height > 0 && probe.plan.swapchain.imageCount >= MinFramesInFlight);
        probe.framesInFlightCompatible = probe.plan.frames.size() == probe.plan.config.framesInFlight;

        probe.warnings = probe.plan.warnings;
        if (tryLoadSystemVulkan && (!probe.loader.loaded || !probe.loader.vkGetInstanceProcAddrResolved))
        {
            ++probe.warnings;
        }

        probe.ok = probe.plan.valid && probe.renderGraphCompatible && probe.policyCompatible && probe.swapchainCompatible && probe.framesInFlightCompatible;

        std::ostringstream out;
        out << "vulkan probe ok=" << (probe.ok ? "true" : "false")
            << " loader=" << (probe.loader.loaded ? "loaded" : "not_loaded")
            << " frame_policy=" << (probe.framesInFlightCompatible ? "ok" : "bad")
            << " graph=" << (probe.renderGraphCompatible ? "ok" : "bad")
            << " swapchain=" << (probe.swapchainCompatible ? "ok" : "bad")
            << " warnings=" << probe.warnings;
        probe.summary = out.str();
        return probe;
    }

    VulkanRuntimeResult RunVulkanRuntimeBootstrap(const VulkanRhiConfig& config, bool submitClearFrame)
    {
        VulkanRuntimeResult result{};
        result.attempted = true;
        result.lastStage = VulkanRuntimeStage::Loader;

        VulkanRuntimeLoader loader{};
        VulkanRuntimeObjects objects{};

        if (!LoadVulkanLibrary(loader))
        {
            result.message = "Vulkan loader or global entry points are unavailable";
            result.lastStage = VulkanRuntimeStage::Failed;
            UnloadVulkanLibrary(loader);
            return result;
        }
        result.loaderLoaded = true;

        VulkanRhiPlan plan = BuildVulkanRhiPlan(config);
        result.headless = plan.headless;
        result.swapchainSkipped = plan.headless;

        std::vector<std::string> instanceExtensions = plan.instanceExtensions;
        std::vector<std::string> validationLayers = plan.validationLayers;
        const std::vector<std::string> availableInstanceExtensions = EnumerateInstanceExtensions(loader);
        const std::vector<std::string> availableInstanceLayers = EnumerateInstanceLayers(loader);

        for (const std::string& extension : instanceExtensions)
        {
            if (!ContainsName(availableInstanceExtensions, extension))
            {
                ++result.warnings;
                if (IsRequiredSurfaceExtension(config, extension))
                {
                    result.message = "required Vulkan surface instance extension is unavailable: " + extension;
                    result.lastStage = VulkanRuntimeStage::Failed;
                    UnloadVulkanLibrary(loader);
                    return result;
                }
            }
        }
        FilterUnavailable(instanceExtensions, availableInstanceExtensions, result.warnings, false);
        FilterUnavailable(validationLayers, availableInstanceLayers, result.warnings, false);
        result.validationEnabled = !validationLayers.empty();

        const std::vector<const char*> instanceExtensionNames = ToCStringPointers(instanceExtensions);
        const std::vector<const char*> validationLayerNames = ToCStringPointers(validationLayers);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = config.applicationName.c_str();
        appInfo.applicationVersion = MakeVkVersion(config.applicationVersionMajor, config.applicationVersionMinor, config.applicationVersionPatch);
        appInfo.pEngineName = config.engineName.c_str();
        appInfo.engineVersion = MakeVkVersion(AK_ENGINE_VERSION_MAJOR, AK_ENGINE_VERSION_MINOR, AK_ENGINE_VERSION_PATCH);
        appInfo.apiVersion = MakeVkVersion(1, 0, 0);

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledLayerCount = static_cast<u32>(validationLayerNames.size());
        instanceInfo.ppEnabledLayerNames = validationLayerNames.empty() ? nullptr : validationLayerNames.data();
        instanceInfo.enabledExtensionCount = static_cast<u32>(instanceExtensionNames.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensionNames.empty() ? nullptr : instanceExtensionNames.data();

        VkResult vk = loader.vkCreateInstance(&instanceInfo, nullptr, &objects.instance);
        if (vk != VK_SUCCESS)
        {
            result.vulkanResult = vk;
            result.message = "vkCreateInstance failed";
            result.lastStage = VulkanRuntimeStage::Failed;
            UnloadVulkanLibrary(loader);
            return result;
        }
        result.instanceCreated = true;
        result.lastStage = VulkanRuntimeStage::Instance;
        LoadInstanceFunctions(loader, objects.instance);

        if (!plan.headless)
        {
            vk = CreatePlatformSurface(loader, config, objects.instance, &objects.surface);
            if (vk != VK_SUCCESS || objects.surface == VK_NULL_HANDLE_U64)
            {
                result.vulkanResult = vk;
                result.message = "platform surface creation failed";
                result.lastStage = VulkanRuntimeStage::Failed;
                CleanupRuntimeObjects(loader, objects);
                UnloadVulkanLibrary(loader);
                return result;
            }
            result.surfaceCreated = true;
            result.lastStage = VulkanRuntimeStage::Surface;
        }

        u32 physicalDeviceCount = 0;
        vk = loader.vkEnumeratePhysicalDevices(objects.instance, &physicalDeviceCount, nullptr);
        if (vk != VK_SUCCESS || physicalDeviceCount == 0)
        {
            result.vulkanResult = vk;
            result.message = "no Vulkan physical devices found";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }
        result.physicalDeviceCount = physicalDeviceCount;
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vk = loader.vkEnumeratePhysicalDevices(objects.instance, &physicalDeviceCount, physicalDevices.data());
        if (vk != VK_SUCCESS && vk != VK_INCOMPLETE)
        {
            result.vulkanResult = vk;
            result.message = "vkEnumeratePhysicalDevices failed";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }

        for (u32 deviceIndex = 0; deviceIndex < physicalDeviceCount; ++deviceIndex)
        {
            u32 graphicsFamily = InvalidQueueFamily;
            u32 presentFamily = InvalidQueueFamily;
            if (!FindQueueFamilies(loader, physicalDevices[deviceIndex], objects.surface, !plan.headless, graphicsFamily, presentFamily))
            {
                continue;
            }

            const std::vector<std::string> deviceExtensions = EnumerateDeviceExtensions(loader, physicalDevices[deviceIndex]);
            if (!plan.headless && !ContainsName(deviceExtensions, "VK_KHR_swapchain"))
            {
                continue;
            }

            objects.physicalDevice = physicalDevices[deviceIndex];
            result.graphicsQueueFamily = graphicsFamily;
            result.presentQueueFamily = presentFamily;
            result.adapterName = "VulkanPhysicalDevice#" + std::to_string(deviceIndex);
            break;
        }

        if (!objects.physicalDevice)
        {
            result.message = "no physical device supports required graphics/present queues and extensions";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }
        result.physicalDeviceSelected = true;
        result.lastStage = VulkanRuntimeStage::PhysicalDevice;

        const float queuePriority = 1.0f;
        std::array<VkDeviceQueueCreateInfo, 2> queueInfos{};
        u32 queueInfoCount = 0;
        queueInfos[queueInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = result.graphicsQueueFamily;
        queueInfos[queueInfoCount].queueCount = 1;
        queueInfos[queueInfoCount].pQueuePriorities = &queuePriority;
        ++queueInfoCount;

        if (!plan.headless && result.presentQueueFamily != result.graphicsQueueFamily)
        {
            queueInfos[queueInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfos[queueInfoCount].queueFamilyIndex = result.presentQueueFamily;
            queueInfos[queueInfoCount].queueCount = 1;
            queueInfos[queueInfoCount].pQueuePriorities = &queuePriority;
            ++queueInfoCount;
        }

        std::vector<std::string> runtimeDeviceExtensions;
        if (!plan.headless)
        {
            runtimeDeviceExtensions.push_back("VK_KHR_swapchain");
        }
        const std::vector<const char*> runtimeDeviceExtensionNames = ToCStringPointers(runtimeDeviceExtensions);

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = queueInfoCount;
        deviceInfo.pQueueCreateInfos = queueInfos.data();
        deviceInfo.enabledExtensionCount = static_cast<u32>(runtimeDeviceExtensionNames.size());
        deviceInfo.ppEnabledExtensionNames = runtimeDeviceExtensionNames.empty() ? nullptr : runtimeDeviceExtensionNames.data();

        vk = loader.vkCreateDevice(objects.physicalDevice, &deviceInfo, nullptr, &objects.device);
        if (vk != VK_SUCCESS)
        {
            result.vulkanResult = vk;
            result.message = "vkCreateDevice failed";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }
        result.deviceCreated = true;
        result.lastStage = VulkanRuntimeStage::LogicalDevice;
        LoadDeviceFunctions(loader, objects.device);
        if (!loader.vkGetDeviceQueue || !loader.vkDestroyDevice || !loader.vkDeviceWaitIdle)
        {
            result.message = "required Vulkan device entry points are unavailable";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }
        if (!plan.headless && (!loader.vkCreateSwapchainKHR || !loader.vkAcquireNextImageKHR || !loader.vkQueuePresentKHR))
        {
            result.message = "required Vulkan swapchain entry points are unavailable";
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }
        loader.vkGetDeviceQueue(objects.device, result.graphicsQueueFamily, 0, &objects.graphicsQueue);
        loader.vkGetDeviceQueue(objects.device, result.presentQueueFamily, 0, &objects.presentQueue);

        if (!CreateCommandAndSync(loader, objects, result.graphicsQueueFamily, result))
        {
            result.lastStage = VulkanRuntimeStage::Failed;
            CleanupRuntimeObjects(loader, objects);
            UnloadVulkanLibrary(loader);
            return result;
        }

        if (!plan.headless)
        {
            VkSurfaceCapabilitiesKHR caps{};
            vk = loader.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(objects.physicalDevice, objects.surface, &caps);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed";
                result.lastStage = VulkanRuntimeStage::Failed;
                CleanupRuntimeObjects(loader, objects);
                UnloadVulkanLibrary(loader);
                return result;
            }

            u32 formatCount = 0;
            loader.vkGetPhysicalDeviceSurfaceFormatsKHR(objects.physicalDevice, objects.surface, &formatCount, nullptr);
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            if (formatCount > 0)
            {
                loader.vkGetPhysicalDeviceSurfaceFormatsKHR(objects.physicalDevice, objects.surface, &formatCount, formats.data());
            }

            u32 presentModeCount = 0;
            loader.vkGetPhysicalDeviceSurfacePresentModesKHR(objects.physicalDevice, objects.surface, &presentModeCount, nullptr);
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            if (presentModeCount > 0)
            {
                loader.vkGetPhysicalDeviceSurfacePresentModesKHR(objects.physicalDevice, objects.surface, &presentModeCount, presentModes.data());
            }

            const VkFormat chosenFormat = ChooseSurfaceFormat(formats);
            const VkColorSpaceKHR colorSpace = ChooseColorSpace(formats, chosenFormat);
            const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes, config.presentModePolicy, result);
            const VkExtent2D extent = ChooseExtent(caps, config);
            u32 imageCount = std::max(caps.minImageCount, std::clamp(config.framesInFlight, MinFramesInFlight, MaxFramesInFlight));
            if (caps.maxImageCount > 0)
            {
                imageCount = std::min(imageCount, caps.maxImageCount);
            }

            const bool supportsTransferClear = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
            if (!supportsTransferClear)
            {
                ++result.warnings;
            }

            std::array<u32, 2> queueFamilies{result.graphicsQueueFamily, result.presentQueueFamily};
            VkSwapchainCreateInfoKHR swapInfo{};
            swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapInfo.surface = objects.surface;
            swapInfo.minImageCount = imageCount;
            swapInfo.imageFormat = chosenFormat;
            swapInfo.imageColorSpace = colorSpace;
            swapInfo.imageExtent = extent;
            swapInfo.imageArrayLayers = 1;
            swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (supportsTransferClear ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0u);
            swapInfo.imageSharingMode = result.graphicsQueueFamily == result.presentQueueFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
            swapInfo.queueFamilyIndexCount = result.graphicsQueueFamily == result.presentQueueFamily ? 0u : 2u;
            swapInfo.pQueueFamilyIndices = result.graphicsQueueFamily == result.presentQueueFamily ? nullptr : queueFamilies.data();
            swapInfo.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
            swapInfo.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha);
            swapInfo.presentMode = presentMode;
            swapInfo.clipped = 1;

            vk = loader.vkCreateSwapchainKHR(objects.device, &swapInfo, nullptr, &objects.swapchain);
            if (vk != VK_SUCCESS)
            {
                result.vulkanResult = vk;
                result.message = "vkCreateSwapchainKHR failed";
                result.lastStage = VulkanRuntimeStage::Failed;
                CleanupRuntimeObjects(loader, objects);
                UnloadVulkanLibrary(loader);
                return result;
            }
            result.swapchainCreated = true;
            result.swapchainWidth = extent.width;
            result.swapchainHeight = extent.height;
            result.colorFormat = FormatName(chosenFormat);
            result.presentMode = PresentModeName(presentMode);
            result.lastStage = VulkanRuntimeStage::Swapchain;

            u32 swapchainImageCount = 0;
            loader.vkGetSwapchainImagesKHR(objects.device, objects.swapchain, &swapchainImageCount, nullptr);
            objects.swapchainImages.resize(swapchainImageCount);
            loader.vkGetSwapchainImagesKHR(objects.device, objects.swapchain, &swapchainImageCount, objects.swapchainImages.data());
            result.swapchainImageCount = swapchainImageCount;

            objects.imageViews.resize(objects.swapchainImages.size());
            for (usize i = 0; i < objects.swapchainImages.size(); ++i)
            {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = objects.swapchainImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = chosenFormat;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;
                vk = loader.vkCreateImageView(objects.device, &viewInfo, nullptr, &objects.imageViews[i]);
                if (vk != VK_SUCCESS)
                {
                    result.vulkanResult = vk;
                    result.message = "vkCreateImageView failed";
                    result.lastStage = VulkanRuntimeStage::Failed;
                    CleanupRuntimeObjects(loader, objects);
                    UnloadVulkanLibrary(loader);
                    return result;
                }
            }
            result.imageViewsCreated = !objects.imageViews.empty();
            result.lastStage = VulkanRuntimeStage::ImageViews;

            if (submitClearFrame && supportsTransferClear)
            {
                if (!SubmitClearFrame(loader, objects, result))
                {
                    result.lastStage = VulkanRuntimeStage::Failed;
                    CleanupRuntimeObjects(loader, objects);
                    UnloadVulkanLibrary(loader);
                    return result;
                }
            }
            else if (submitClearFrame)
            {
                ++result.warnings;
                result.message = "swapchain does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT; clear frame skipped until dynamic-rendering path lands";
            }
        }
        else
        {
            result.swapchainSkipped = true;
            result.message = "headless Vulkan device bootstrap completed; swapchain skipped because no native surface was provided";
        }

        result.ok = result.loaderLoaded
            && result.instanceCreated
            && result.physicalDeviceSelected
            && result.deviceCreated
            && (plan.headless || (result.surfaceCreated && result.swapchainCreated && result.imageViewsCreated && (!submitClearFrame || result.clearSubmitted)));
        result.lastStage = result.ok ? VulkanRuntimeStage::Completed : VulkanRuntimeStage::Failed;

        std::ostringstream out;
        out << "vulkan runtime ok=" << (result.ok ? "true" : "false")
            << " headless=" << (result.headless ? "true" : "false")
            << " instance=" << (result.instanceCreated ? "yes" : "no")
            << " device=" << (result.deviceCreated ? "yes" : "no")
            << " surface=" << (result.surfaceCreated ? "yes" : "no")
            << " swapchain=" << (result.swapchainCreated ? "yes" : "no")
            << " clear=" << (result.clearSubmitted ? "yes" : "no")
            << " present=" << (result.presented ? "yes" : "no")
            << " warnings=" << result.warnings;
        result.summary = out.str();

        CleanupRuntimeObjects(loader, objects);
        UnloadVulkanLibrary(loader);
        return result;
    }

    VulkanVertexLayoutDesc BuildDefaultVulkanMeshVertexLayout()
    {
        VulkanVertexLayoutDesc layout{};
        layout.strideBytes = 48;
        layout.attributes.push_back({"POSITION", 0, VulkanVertexFormat::Float3, 0});
        layout.attributes.push_back({"NORMAL", 1, VulkanVertexFormat::Float3, 12});
        layout.attributes.push_back({"TEXCOORD0", 2, VulkanVertexFormat::Float2, 24});
        layout.attributes.push_back({"COLOR0", 3, VulkanVertexFormat::Float4, 32});
        layout.valid = true;
        return layout;
    }

    VulkanVertexLayoutDesc BuildPackedVulkanMeshVertexLayout32()
    {
        VulkanVertexLayoutDesc layout{};
        layout.strideBytes = 32;
        layout.attributes.push_back({"POSITION", 0, VulkanVertexFormat::Float3, 0});
        layout.attributes.push_back({"NORMAL_OCT", 1, VulkanVertexFormat::Snorm16x2, 12});
        layout.attributes.push_back({"TEXCOORD0", 2, VulkanVertexFormat::Unorm16x2, 16});
        layout.attributes.push_back({"COLOR0", 3, VulkanVertexFormat::Uint32, 20});
        layout.attributes.push_back({"MATERIAL_FLAGS", 4, VulkanVertexFormat::Uint32, 24});
        layout.valid = true;
        return layout;
    }

    VulkanGraphicsPipelinePlan BuildVulkanPrimitiveGraphicsPipelinePlan(const VulkanRhiConfig& config, const std::string& colorFormat, const std::string& depthFormat)
    {
        VulkanGraphicsPipelinePlan plan{};
        plan.topology = VulkanPrimitiveTopology::TriangleList;
        plan.cullMode = VulkanCullMode::Back;
        plan.depthCompare = VulkanDepthCompare::GreaterOrEqual;
        plan.vertexLayout = BuildDefaultVulkanMeshVertexLayout();
        plan.colorAttachment = !colorFormat.empty();
        plan.depthAttachment = !depthFormat.empty();
        plan.reversedZ = true;
        plan.dynamicViewport = true;
        plan.dynamicScissor = true;
        plan.alphaBlend = false;
        plan.wireframeAllowed = true;

        plan.vertexShader.name = "ak_primitive_mesh_vs";
        plan.vertexShader.stage = "vertex";
        plan.vertexShader.sourceLanguage = "Slang/HLSL -> SPIR-V";
        plan.vertexShader.bytecodePath = ".akcache/shaders/ak_primitive_mesh_vs.spv";
        plan.vertexShader.embeddedFallback = true;
        plan.vertexShader.valid = true;

        plan.fragmentShader.name = "ak_primitive_mesh_fs";
        plan.fragmentShader.stage = "fragment";
        plan.fragmentShader.sourceLanguage = "Slang/HLSL -> SPIR-V";
        plan.fragmentShader.bytecodePath = ".akcache/shaders/ak_primitive_mesh_fs.spv";
        plan.fragmentShader.embeddedFallback = true;
        plan.fragmentShader.valid = true;

        if (!config.requireDynamicRendering)
        {
            ++plan.warnings;
        }
        if (config.framesInFlight < MinFramesInFlight || config.framesInFlight > MaxFramesInFlight)
        {
            ++plan.warnings;
        }
        if (!plan.colorAttachment || !plan.depthAttachment)
        {
            ++plan.warnings;
        }

        plan.valid = plan.vertexLayout.valid
            && plan.vertexShader.valid
            && plan.fragmentShader.valid
            && plan.colorAttachment
            && plan.depthAttachment
            && plan.reversedZ
            && plan.dynamicViewport
            && plan.dynamicScissor;

        std::ostringstream out;
        out << "vulkan graphics pipeline plan topology=" << ToString(plan.topology)
            << " vertex_stride=" << plan.vertexLayout.strideBytes
            << " attributes=" << plan.vertexLayout.attributes.size()
            << " color=" << colorFormat
            << " depth=" << depthFormat
            << " reversed_z=" << (plan.reversedZ ? "true" : "false")
            << " dynamic_rendering=" << (config.requireDynamicRendering ? "true" : "false")
            << " valid=" << (plan.valid ? "true" : "false")
            << " warnings=" << plan.warnings;
        plan.summary = out.str();
        return plan;
    }

    VulkanPipelineValidationReport ValidateVulkanGraphicsPipelinePlan(const VulkanGraphicsPipelinePlan& plan)
    {
        VulkanPipelineValidationReport report{};
        report.vertexLayoutOk = plan.vertexLayout.valid
            && plan.vertexLayout.strideBytes >= 12
            && plan.vertexLayout.attributes.size() >= 3;
        report.shadersOk = plan.vertexShader.valid && plan.fragmentShader.valid
            && !plan.vertexShader.name.empty()
            && !plan.fragmentShader.name.empty();
        report.attachmentsOk = plan.colorAttachment && plan.depthAttachment;
        report.depthPolicyOk = plan.reversedZ && plan.depthCompare == VulkanDepthCompare::GreaterOrEqual;
        report.warnings = plan.warnings;
        if (!report.vertexLayoutOk) { ++report.warnings; }
        if (!report.shadersOk) { ++report.warnings; }
        if (!report.attachmentsOk) { ++report.warnings; }
        if (!report.depthPolicyOk) { ++report.warnings; }
        report.ok = report.vertexLayoutOk && report.shadersOk && report.attachmentsOk && report.depthPolicyOk;

        std::ostringstream out;
        out << "pipeline_validation ok=" << (report.ok ? "true" : "false")
            << " vertex_layout=" << (report.vertexLayoutOk ? "ok" : "bad")
            << " shaders=" << (report.shadersOk ? "ok" : "bad")
            << " attachments=" << (report.attachmentsOk ? "ok" : "bad")
            << " depth=" << (report.depthPolicyOk ? "ok" : "bad")
            << " warnings=" << report.warnings;
        report.summary = out.str();
        return report;
    }

    VulkanPrimitiveRendererProbe BuildVulkanPrimitiveRendererProbe(const VulkanRhiConfig& config, const std::vector<VulkanDrawCallPlan>& drawCalls)
    {
        VulkanPrimitiveRendererProbe probe{};
        VulkanRhiPlan rhiPlan = BuildVulkanRhiPlan(config);
        probe.pipeline = BuildVulkanPrimitiveGraphicsPipelinePlan(config, rhiPlan.swapchain.colorFormat, rhiPlan.swapchain.depthFormat);
        probe.validation = ValidateVulkanGraphicsPipelinePlan(probe.pipeline);
        probe.drawCalls = drawCalls;
        probe.usesReversedZ = probe.pipeline.reversedZ;

        for (VulkanDrawCallPlan& draw : probe.drawCalls)
        {
            draw.valid = draw.vertexCount > 0 && (!draw.indexed || draw.indexCount > 0) && draw.instanceCount > 0;
            probe.totalVertices += draw.vertexCount * draw.instanceCount;
            probe.totalIndices += draw.indexCount * draw.instanceCount;
            if (draw.indexed)
            {
                probe.totalTriangles += (draw.indexCount / 3u) * draw.instanceCount;
            }
            else
            {
                probe.totalTriangles += (draw.vertexCount / 3u) * draw.instanceCount;
            }
            if (!draw.valid)
            {
                probe.finite = false;
                ++probe.warnings;
            }
        }

        probe.warnings += probe.pipeline.warnings + probe.validation.warnings;
        probe.readyForGpuDraw = probe.validation.ok
            && !probe.drawCalls.empty()
            && probe.totalVertices > 0
            && probe.totalTriangles > 0
            && probe.finite;

        std::ostringstream out;
        out << "vulkan primitive renderer probe ready=" << (probe.readyForGpuDraw ? "true" : "false")
            << " draw_calls=" << probe.drawCalls.size()
            << " vertices=" << probe.totalVertices
            << " indices=" << probe.totalIndices
            << " triangles=" << probe.totalTriangles
            << " reversed_z=" << (probe.usesReversedZ ? "true" : "false")
            << " warnings=" << probe.warnings;
        probe.summary = out.str();
        return probe;
    }

    const char* ToString(RhiBackend backend)
    {
        switch (backend)
        {
            case RhiBackend::Vulkan:
                return "Vulkan";
            case RhiBackend::None:
                return "None";
            default:
                return "Unknown";
        }
    }

    const char* ToString(VulkanWindowPlatform platform)
    {
        switch (platform)
        {
            case VulkanWindowPlatform::Headless:
                return "Headless";
            case VulkanWindowPlatform::Win32:
                return "Win32";
            case VulkanWindowPlatform::Xlib:
                return "Xlib";
            case VulkanWindowPlatform::Xcb:
                return "Xcb";
            case VulkanWindowPlatform::Wayland:
                return "Wayland";
            case VulkanWindowPlatform::Metal:
                return "Metal";
            default:
                return "Unknown";
        }
    }

    const char* ToString(VulkanPresentModePolicy policy)
    {
        switch (policy)
        {
            case VulkanPresentModePolicy::MailboxPreferLowLatency:
                return "mailbox-prefer-low-latency";
            case VulkanPresentModePolicy::FifoAlwaysAvailable:
                return "fifo";
            case VulkanPresentModePolicy::ImmediateAllowTearing:
                return "immediate-allow-tearing";
            default:
                return "unknown";
        }
    }

    const char* ToString(VulkanRuntimeStage stage)
    {
        switch (stage)
        {
            case VulkanRuntimeStage::NotStarted:
                return "NotStarted";
            case VulkanRuntimeStage::Loader:
                return "Loader";
            case VulkanRuntimeStage::Instance:
                return "Instance";
            case VulkanRuntimeStage::Surface:
                return "Surface";
            case VulkanRuntimeStage::PhysicalDevice:
                return "PhysicalDevice";
            case VulkanRuntimeStage::LogicalDevice:
                return "LogicalDevice";
            case VulkanRuntimeStage::Swapchain:
                return "Swapchain";
            case VulkanRuntimeStage::ImageViews:
                return "ImageViews";
            case VulkanRuntimeStage::CommandResources:
                return "CommandResources";
            case VulkanRuntimeStage::SyncObjects:
                return "SyncObjects";
            case VulkanRuntimeStage::ClearFrame:
                return "ClearFrame";
            case VulkanRuntimeStage::Present:
                return "Present";
            case VulkanRuntimeStage::Completed:
                return "Completed";
            case VulkanRuntimeStage::Failed:
                return "Failed";
            default:
                return "Unknown";
        }
    }

    const char* ToString(VulkanPrimitiveTopology topology)
    {
        switch (topology)
        {
            case VulkanPrimitiveTopology::TriangleList: return "TriangleList";
            case VulkanPrimitiveTopology::LineList: return "LineList";
            case VulkanPrimitiveTopology::PointList: return "PointList";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanVertexFormat format)
    {
        switch (format)
        {
            case VulkanVertexFormat::Float2: return "Float2";
            case VulkanVertexFormat::Float3: return "Float3";
            case VulkanVertexFormat::Float4: return "Float4";
            case VulkanVertexFormat::Snorm16x2: return "Snorm16x2";
            case VulkanVertexFormat::Unorm16x2: return "Unorm16x2";
            case VulkanVertexFormat::Uint32: return "Uint32";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanCullMode cullMode)
    {
        switch (cullMode)
        {
            case VulkanCullMode::None: return "None";
            case VulkanCullMode::Back: return "Back";
            case VulkanCullMode::Front: return "Front";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanDepthCompare compare)
    {
        switch (compare)
        {
            case VulkanDepthCompare::GreaterOrEqual: return "GreaterOrEqual";
            case VulkanDepthCompare::LessOrEqual: return "LessOrEqual";
            case VulkanDepthCompare::Always: return "Always";
            default: return "Unknown";
        }
    }

    std::string ToDebugString(const VulkanRhiPlan& plan)
    {
        std::ostringstream out;
        out << plan.summary
            << " swapchain=" << plan.swapchain.width << "x" << plan.swapchain.height
            << " present=" << ToString(plan.swapchain.presentModePolicy)
            << " depth=" << plan.swapchain.depthFormat;
        return out.str();
    }

    std::string ToDebugString(const VulkanLoaderProbe& probe)
    {
        std::ostringstream out;
        out << "loader lib=" << probe.libraryName
            << " loaded=" << (probe.loaded ? "true" : "false")
            << " vkGetInstanceProcAddr=" << (probe.vkGetInstanceProcAddrResolved ? "true" : "false")
            << " message=\"" << probe.message << "\"";
        return out.str();
    }

    std::string ToDebugString(const VulkanRhiProbe& probe)
    {
        std::ostringstream out;
        out << probe.summary
            << " instance_ext=" << probe.plan.instanceExtensions.size()
            << " device_ext=" << probe.plan.deviceExtensions.size()
            << " queues=" << probe.plan.queues.size();
        return out.str();
    }

    std::string ToDebugString(const VulkanRuntimeResult& result)
    {
        std::ostringstream out;
        out << result.summary
            << " stage=" << ToString(result.lastStage)
            << " devices=" << result.physicalDeviceCount
            << " adapter=\"" << result.adapterName << "\""
            << " graphics_q=" << result.graphicsQueueFamily
            << " present_q=" << result.presentQueueFamily
            << " images=" << result.swapchainImageCount
            << " size=" << result.swapchainWidth << "x" << result.swapchainHeight
            << " format=" << (result.colorFormat.empty() ? "n/a" : result.colorFormat)
            << " present_mode=" << (result.presentMode.empty() ? "n/a" : result.presentMode)
            << " validation=" << (result.validationEnabled ? "on" : "off")
            << " transfer_clear=" << (result.usedTransferClear ? "yes" : "no")
            << " fifo_fallback=" << (result.usedFifoFallback ? "yes" : "no")
            << " message=\"" << result.message << "\""
            << " vk=" << result.vulkanResult;
        return out.str();
    }

    std::string ToDebugString(const VulkanGraphicsPipelinePlan& plan)
    {
        std::ostringstream out;
        out << plan.summary
            << " cull=" << ToString(plan.cullMode)
            << " depth_compare=" << ToString(plan.depthCompare)
            << " vs=" << plan.vertexShader.name
            << " fs=" << plan.fragmentShader.name;
        return out.str();
    }

    std::string ToDebugString(const VulkanPipelineValidationReport& report)
    {
        return report.summary;
    }

    std::string ToDebugString(const VulkanPrimitiveRendererProbe& probe)
    {
        std::ostringstream out;
        out << probe.summary
            << " pipeline=" << (probe.pipeline.valid ? "ok" : "bad")
            << " validation=" << (probe.validation.ok ? "ok" : "bad")
            << " finite=" << (probe.finite ? "true" : "false");
        return out.str();
    }
}
