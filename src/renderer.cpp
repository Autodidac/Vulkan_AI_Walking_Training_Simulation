#include "renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace runner::render
{
    namespace
    {
        constexpr std::size_t maximum_vertex_bytes = 8u * 1024u * 1024u;
        constexpr std::size_t frames_in_flight = 2;

        struct QueueFamilies
        {
            std::optional<std::uint32_t> graphics{};
            std::optional<std::uint32_t> present{};
            [[nodiscard]] bool complete() const noexcept { return graphics.has_value() && present.has_value(); }
        };

        [[nodiscard]] std::vector<std::byte> read_binary(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input)
                throw std::runtime_error("Could not open shader: " + path.string());
            const std::streamsize size = input.tellg();
            if (size <= 0 || size % 4 != 0)
                throw std::runtime_error("Invalid SPIR-V shader size: " + path.string());
            input.seekg(0);
            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            input.read(reinterpret_cast<char*>(bytes.data()), size);
            if (!input)
                throw std::runtime_error("Could not read shader: " + path.string());
            return bytes;
        }

        [[nodiscard]] std::string vk_result_name(VkResult result)
        {
            switch (result)
            {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
            }
        }

        void require(VkResult result, std::string_view operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + " failed: " + vk_result_name(result));
        }

        [[nodiscard]] QueueFamilies find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            QueueFamilies result{};
            std::uint32_t count{};
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
            std::vector<VkQueueFamilyProperties> properties(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
            for (std::uint32_t index = 0; index < count; ++index)
            {
                if ((properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                    result.graphics = index;
                VkBool32 supports_present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &supports_present);
                if (supports_present == VK_TRUE)
                    result.present = index;
                if (result.complete())
                    break;
            }
            return result;
        }

        [[nodiscard]] std::uint32_t find_memory_type(
            VkPhysicalDevice physical_device,
            std::uint32_t type_bits,
            VkMemoryPropertyFlags required)
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
            for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
            {
                if ((type_bits & (1u << index)) != 0
                    && (properties.memoryTypes[index].propertyFlags & required) == required)
                    return index;
            }
            throw std::runtime_error("No suitable Vulkan memory type was found.");
        }
    }

    struct VulkanRenderer::Impl
    {
        SDL_Window* window{};
        std::filesystem::path shader_directory{};
        VkInstance instance{};
        VkSurfaceKHR surface{};
        VkPhysicalDevice physical_device{};
        VkDevice device{};
        VkQueue graphics_queue{};
        VkQueue present_queue{};
        std::uint32_t graphics_family{};
        std::uint32_t present_family{};
        VkSwapchainKHR swapchain{};
        VkFormat swapchain_format{ VK_FORMAT_B8G8R8A8_UNORM };
        VkExtent2D extent{};
        std::vector<VkImage> images{};
        std::vector<VkImageView> image_views{};
        VkPipelineLayout pipeline_layout{};
        VkPipeline pipeline{};
        VkCommandPool command_pool{};
        std::array<VkCommandBuffer, frames_in_flight> command_buffers{};
        std::array<VkSemaphore, frames_in_flight> image_available{};
        std::array<VkSemaphore, frames_in_flight> render_finished{};
        std::array<VkFence, frames_in_flight> in_flight{};
        std::array<VkBuffer, frames_in_flight> vertex_buffers{};
        std::array<VkDeviceMemory, frames_in_flight> vertex_memories{};
        std::array<void*, frames_in_flight> mapped_vertices{};
        std::size_t frame_index{};

        void destroy_swapchain() noexcept
        {
            if (device == VK_NULL_HANDLE)
                return;
            if (pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
            for (VkImageView view : image_views)
                vkDestroyImageView(device, view, nullptr);
            image_views.clear();
            images.clear();
            if (swapchain != VK_NULL_HANDLE)
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }

        void create_swapchain(int drawable_width, int drawable_height)
        {
            VkSurfaceCapabilitiesKHR capabilities{};
            require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

            std::uint32_t format_count{};
            require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
            std::vector<VkSurfaceFormatKHR> formats(format_count);
            require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
            VkSurfaceFormatKHR chosen = formats.front();
            for (const VkSurfaceFormatKHR candidate : formats)
            {
                if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    chosen = candidate;
                    break;
                }
            }
            swapchain_format = chosen.format;

            if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
                extent = capabilities.currentExtent;
            else
            {
                extent.width = std::clamp(static_cast<std::uint32_t>(std::max(drawable_width, 1)), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                extent.height = std::clamp(static_cast<std::uint32_t>(std::max(drawable_height, 1)), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            }

            std::uint32_t image_count = std::max(3u, capabilities.minImageCount + 1u);
            if (capabilities.maxImageCount > 0)
                image_count = std::min(image_count, capabilities.maxImageCount);

            const std::array<std::uint32_t, 2> families{ graphics_family, present_family };
            VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
            create_info.surface = surface;
            create_info.minImageCount = image_count;
            create_info.imageFormat = swapchain_format;
            create_info.imageColorSpace = chosen.colorSpace;
            create_info.imageExtent = extent;
            create_info.imageArrayLayers = 1;
            create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (graphics_family != present_family)
            {
                create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                create_info.queueFamilyIndexCount = static_cast<std::uint32_t>(families.size());
                create_info.pQueueFamilyIndices = families.data();
            }
            else
                create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.preTransform = capabilities.currentTransform;
            create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            create_info.clipped = VK_TRUE;
            require(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain), "vkCreateSwapchainKHR");

            require(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr), "vkGetSwapchainImagesKHR");
            images.resize(image_count);
            require(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data()), "vkGetSwapchainImagesKHR");
            image_views.reserve(images.size());
            for (VkImage image : images)
            {
                VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
                view_info.image = image;
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = swapchain_format;
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.levelCount = 1;
                view_info.subresourceRange.layerCount = 1;
                VkImageView view{};
                require(vkCreateImageView(device, &view_info, nullptr, &view), "vkCreateImageView");
                image_views.push_back(view);
            }
            create_pipeline();
        }

        [[nodiscard]] VkShaderModule create_shader_module(const std::filesystem::path& path)
        {
            const std::vector<std::byte> bytes = read_binary(path);
            VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            create_info.codeSize = bytes.size();
            create_info.pCode = reinterpret_cast<const std::uint32_t*>(bytes.data());
            VkShaderModule module{};
            require(vkCreateShaderModule(device, &create_info, nullptr, &module), "vkCreateShaderModule");
            return module;
        }

        void create_pipeline()
        {
            const VkShaderModule vertex_shader = create_shader_module(shader_directory / "flat.vert.spv");
            const VkShaderModule fragment_shader = create_shader_module(shader_directory / "flat.frag.spv");
            const std::array<VkPipelineShaderStageCreateInfo, 2> stages{
                VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "main", nullptr },
                VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "main", nullptr }
            };

            const VkVertexInputBindingDescription binding{ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
            const std::array<VkVertexInputAttributeDescription, 2> attributes{
                VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, position)) },
                VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, color)) }
            };
            VkPipelineVertexInputStateCreateInfo vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            vertex_input.vertexBindingDescriptionCount = 1;
            vertex_input.pVertexBindingDescriptions = &binding;
            vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
            vertex_input.pVertexAttributeDescriptions = attributes.data();

            VkPipelineInputAssemblyStateCreateInfo assembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterization{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization.cullMode = VK_CULL_MODE_NONE;
            rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState attachment{};
            attachment.blendEnable = VK_TRUE;
            attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp = VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            blend.attachmentCount = 1;
            blend.pAttachments = &attachment;

            const std::array<VkDynamicState, 2> dynamic_states{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
            dynamic.pDynamicStates = dynamic_states.data();

            struct PushConstants { float width; float height; };
            const VkPushConstantRange push_range{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants) };
            if (pipeline_layout == VK_NULL_HANDLE)
            {
                VkPipelineLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
                layout_info.pushConstantRangeCount = 1;
                layout_info.pPushConstantRanges = &push_range;
                require(vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout), "vkCreatePipelineLayout");
            }

            VkPipelineRenderingCreateInfo rendering_info{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachmentFormats = &swapchain_format;
            VkGraphicsPipelineCreateInfo pipeline_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            pipeline_info.pNext = &rendering_info;
            pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
            pipeline_info.pStages = stages.data();
            pipeline_info.pVertexInputState = &vertex_input;
            pipeline_info.pInputAssemblyState = &assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &rasterization;
            pipeline_info.pMultisampleState = &multisample;
            pipeline_info.pColorBlendState = &blend;
            pipeline_info.pDynamicState = &dynamic;
            pipeline_info.layout = pipeline_layout;
            require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline), "vkCreateGraphicsPipelines");

            vkDestroyShaderModule(device, fragment_shader, nullptr);
            vkDestroyShaderModule(device, vertex_shader, nullptr);
        }

        void recreate_swapchain(int width, int height)
        {
            if (width <= 0 || height <= 0)
                return;
            vkDeviceWaitIdle(device);
            destroy_swapchain();
            create_swapchain(width, height);
        }
    };

    VulkanRenderer::~VulkanRenderer()
    {
        shutdown();
    }

    bool VulkanRenderer::initialize(SDL_Window* window, const std::filesystem::path& shader_directory, std::string& error)
    {
        shutdown();
        impl_ = new Impl{};
        try
        {
            Impl& impl = *impl_;
            impl.window = window;
            impl.shader_directory = shader_directory;

            Uint32 extension_count{};
            const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
            if (extensions == nullptr)
                throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());

            VkApplicationInfo application{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
            application.pApplicationName = "Runner";
            application.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
            application.pEngineName = "RunnerGui";
            application.engineVersion = VK_MAKE_API_VERSION(0, 0, 88, 70);
            application.apiVersion = VK_API_VERSION_1_3;
            VkInstanceCreateInfo instance_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            instance_info.pApplicationInfo = &application;
            instance_info.enabledExtensionCount = extension_count;
            instance_info.ppEnabledExtensionNames = extensions;
            require(vkCreateInstance(&instance_info, nullptr, &impl.instance), "vkCreateInstance");

            if (!SDL_Vulkan_CreateSurface(window, impl.instance, nullptr, &impl.surface))
                throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());

            std::uint32_t device_count{};
            require(vkEnumeratePhysicalDevices(impl.instance, &device_count, nullptr), "vkEnumeratePhysicalDevices");
            if (device_count == 0)
                throw std::runtime_error("No Vulkan 1.3 physical device is available.");
            std::vector<VkPhysicalDevice> devices(device_count);
            require(vkEnumeratePhysicalDevices(impl.instance, &device_count, devices.data()), "vkEnumeratePhysicalDevices");
            int best_score = std::numeric_limits<int>::min();
            QueueFamilies best_families{};
            for (VkPhysicalDevice candidate : devices)
            {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(candidate, &properties);
                if (properties.apiVersion < VK_API_VERSION_1_3)
                    continue;
                const QueueFamilies families = find_queue_families(candidate, impl.surface);
                if (!families.complete())
                    continue;
                int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000 : 100;
                score += static_cast<int>(properties.limits.maxImageDimension2D / 1024u);
                if (score > best_score)
                {
                    best_score = score;
                    impl.physical_device = candidate;
                    best_families = families;
                }
            }
            if (impl.physical_device == VK_NULL_HANDLE)
                throw std::runtime_error("No Vulkan 1.3 device with graphics and presentation support was found.");
            impl.graphics_family = *best_families.graphics;
            impl.present_family = *best_families.present;

            const std::set<std::uint32_t> unique_families{ impl.graphics_family, impl.present_family };
            std::vector<VkDeviceQueueCreateInfo> queue_infos{};
            constexpr float priority = 1.0f;
            for (std::uint32_t family : unique_families)
            {
                VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
                queue_info.queueFamilyIndex = family;
                queue_info.queueCount = 1;
                queue_info.pQueuePriorities = &priority;
                queue_infos.push_back(queue_info);
            }
            VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
            features13.dynamicRendering = VK_TRUE;
            const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            VkDeviceCreateInfo device_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            device_info.pNext = &features13;
            device_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.enabledExtensionCount = 1;
            device_info.ppEnabledExtensionNames = device_extensions;
            require(vkCreateDevice(impl.physical_device, &device_info, nullptr, &impl.device), "vkCreateDevice");
            vkGetDeviceQueue(impl.device, impl.graphics_family, 0, &impl.graphics_queue);
            vkGetDeviceQueue(impl.device, impl.present_family, 0, &impl.present_queue);

            VkCommandPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = impl.graphics_family;
            require(vkCreateCommandPool(impl.device, &pool_info, nullptr, &impl.command_pool), "vkCreateCommandPool");
            VkCommandBufferAllocateInfo allocation{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocation.commandPool = impl.command_pool;
            allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocation.commandBufferCount = static_cast<std::uint32_t>(impl.command_buffers.size());
            require(vkAllocateCommandBuffers(impl.device, &allocation, impl.command_buffers.data()), "vkAllocateCommandBuffers");

            for (std::size_t index = 0; index < frames_in_flight; ++index)
            {
                const VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                require(vkCreateSemaphore(impl.device, &semaphore_info, nullptr, &impl.image_available[index]), "vkCreateSemaphore");
                require(vkCreateSemaphore(impl.device, &semaphore_info, nullptr, &impl.render_finished[index]), "vkCreateSemaphore");
                VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                require(vkCreateFence(impl.device, &fence_info, nullptr, &impl.in_flight[index]), "vkCreateFence");
            }

            for (std::size_t frame = 0; frame < frames_in_flight; ++frame)
            {
                VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
                buffer_info.size = maximum_vertex_bytes;
                buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                require(vkCreateBuffer(impl.device, &buffer_info, nullptr, &impl.vertex_buffers[frame]), "vkCreateBuffer");
                VkMemoryRequirements memory_requirements{};
                vkGetBufferMemoryRequirements(impl.device, impl.vertex_buffers[frame], &memory_requirements);
                VkMemoryAllocateInfo memory_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
                memory_info.allocationSize = memory_requirements.size;
                memory_info.memoryTypeIndex = find_memory_type(impl.physical_device, memory_requirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                require(vkAllocateMemory(impl.device, &memory_info, nullptr, &impl.vertex_memories[frame]), "vkAllocateMemory");
                require(vkBindBufferMemory(impl.device, impl.vertex_buffers[frame], impl.vertex_memories[frame], 0), "vkBindBufferMemory");
                require(vkMapMemory(impl.device, impl.vertex_memories[frame], 0, maximum_vertex_bytes, 0, &impl.mapped_vertices[frame]), "vkMapMemory");
            }

            int width{};
            int height{};
            SDL_GetWindowSizeInPixels(window, &width, &height);
            impl.create_swapchain(width, height);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            shutdown();
            return false;
        }
    }

    void VulkanRenderer::shutdown() noexcept
    {
        if (impl_ == nullptr)
            return;
        Impl& impl = *impl_;
        if (impl.device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(impl.device);
        impl.destroy_swapchain();
        if (impl.device != VK_NULL_HANDLE)
        {
            for (std::size_t index = 0; index < frames_in_flight; ++index)
            {
                if (impl.mapped_vertices[index] != nullptr)
                    vkUnmapMemory(impl.device, impl.vertex_memories[index]);
                if (impl.vertex_buffers[index] != VK_NULL_HANDLE)
                    vkDestroyBuffer(impl.device, impl.vertex_buffers[index], nullptr);
                if (impl.vertex_memories[index] != VK_NULL_HANDLE)
                    vkFreeMemory(impl.device, impl.vertex_memories[index], nullptr);
                if (impl.image_available[index] != VK_NULL_HANDLE)
                    vkDestroySemaphore(impl.device, impl.image_available[index], nullptr);
                if (impl.render_finished[index] != VK_NULL_HANDLE)
                    vkDestroySemaphore(impl.device, impl.render_finished[index], nullptr);
                if (impl.in_flight[index] != VK_NULL_HANDLE)
                    vkDestroyFence(impl.device, impl.in_flight[index], nullptr);
            }
            if (impl.command_pool != VK_NULL_HANDLE)
                vkDestroyCommandPool(impl.device, impl.command_pool, nullptr);
            if (impl.pipeline_layout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(impl.device, impl.pipeline_layout, nullptr);
            vkDestroyDevice(impl.device, nullptr);
        }
        if (impl.surface != VK_NULL_HANDLE && impl.instance != VK_NULL_HANDLE)
            SDL_Vulkan_DestroySurface(impl.instance, impl.surface, nullptr);
        if (impl.instance != VK_NULL_HANDLE)
            vkDestroyInstance(impl.instance, nullptr);
        delete impl_;
        impl_ = nullptr;
    }

    bool VulkanRenderer::render(std::span<const Vertex> vertices, int drawable_width, int drawable_height, std::string& error)
    {
        if (impl_ == nullptr)
        {
            error = "Vulkan renderer is not initialized.";
            return false;
        }
        if (drawable_width <= 0 || drawable_height <= 0)
            return true;
        if (vertices.size_bytes() > maximum_vertex_bytes)
        {
            error = "Frame exceeded the 8 MiB Vulkan vertex budget.";
            return false;
        }

        try
        {
            Impl& impl = *impl_;
            if (impl.extent.width != static_cast<std::uint32_t>(drawable_width)
                || impl.extent.height != static_cast<std::uint32_t>(drawable_height))
                impl.recreate_swapchain(drawable_width, drawable_height);

            const std::size_t frame = impl.frame_index % frames_in_flight;
            require(vkWaitForFences(impl.device, 1, &impl.in_flight[frame], VK_TRUE, UINT64_MAX), "vkWaitForFences");
            std::uint32_t image_index{};
            const VkResult acquire = vkAcquireNextImageKHR(impl.device, impl.swapchain, UINT64_MAX,
                impl.image_available[frame], VK_NULL_HANDLE, &image_index);
            if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
            {
                impl.recreate_swapchain(drawable_width, drawable_height);
                return true;
            }
            if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
                require(acquire, "vkAcquireNextImageKHR");
            require(vkResetFences(impl.device, 1, &impl.in_flight[frame]), "vkResetFences");
            require(vkResetCommandBuffer(impl.command_buffers[frame], 0), "vkResetCommandBuffer");
            if (!vertices.empty())
                std::memcpy(impl.mapped_vertices[frame], vertices.data(), vertices.size_bytes());

            VkCommandBuffer command_buffer = impl.command_buffers[frame];
            VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            require(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");

            VkImageMemoryBarrier to_color{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            to_color.srcAccessMask = 0;
            to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            to_color.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color.image = impl.images[image_index];
            to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_color.subresourceRange.levelCount = 1;
            to_color.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, 0, nullptr, 0, nullptr, 1, &to_color);

            const VkClearValue clear{ { { 0.032f, 0.039f, 0.052f, 1.0f } } };
            VkRenderingAttachmentInfo color_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            color_attachment.imageView = impl.image_views[image_index];
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.clearValue = clear;
            VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            rendering_info.renderArea.extent = impl.extent;
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;
            vkCmdBeginRendering(command_buffer, &rendering_info);

            const VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(impl.extent.width), static_cast<float>(impl.extent.height), 0.0f, 1.0f };
            const VkRect2D scissor{ { 0, 0 }, impl.extent };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, impl.pipeline);
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &impl.vertex_buffers[frame], &offset);
            struct PushConstants { float width; float height; } push{ static_cast<float>(impl.extent.width), static_cast<float>(impl.extent.height) };
            vkCmdPushConstants(command_buffer, impl.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
            if (!vertices.empty())
                vkCmdDraw(command_buffer, static_cast<std::uint32_t>(vertices.size()), 1, 0, 0);
            vkCmdEndRendering(command_buffer);

            VkImageMemoryBarrier to_present{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            to_present.dstAccessMask = 0;
            to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_present.image = impl.images[image_index];
            to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_present.subresourceRange.levelCount = 1;
            to_present.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &to_present);
            require(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

            const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &impl.image_available[frame];
            submit.pWaitDstStageMask = &wait_stage;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command_buffer;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &impl.render_finished[frame];
            require(vkQueueSubmit(impl.graphics_queue, 1, &submit, impl.in_flight[frame]), "vkQueueSubmit");

            VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &impl.render_finished[frame];
            present.swapchainCount = 1;
            present.pSwapchains = &impl.swapchain;
            present.pImageIndices = &image_index;
            const VkResult present_result = vkQueuePresentKHR(impl.present_queue, &present);
            if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
                impl.recreate_swapchain(drawable_width, drawable_height);
            else
                require(present_result, "vkQueuePresentKHR");
            ++impl.frame_index;
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    void VulkanRenderer::wait_idle() noexcept
    {
        if (impl_ != nullptr && impl_->device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(impl_->device);
    }
}
