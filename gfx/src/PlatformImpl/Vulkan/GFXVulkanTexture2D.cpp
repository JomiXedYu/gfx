#include "GFXVulkanTexture2D.h"
#include "BufferHelper.h"
#include <stdexcept>
#include <stb_image.h>
#include "GFXVulkanCommandBuffer.h"
#include <cassert>

namespace gfx
{

    static VkCommandBuffer _GetVkCommandBuffer(const gfx::GFXCommandBufferScope& scope)
    {
        return static_cast<gfx::GFXVulkanCommandBuffer*>(scope.operator->())->GetVkCommandBuffer();
    }

    static void _CreateImage(
        GFXVulkanApplication* app,
        uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


        VkFormatProperties formatProp;
        vkGetPhysicalDeviceFormatProperties(app->GetVkPhysicalDevice(), VkFormat::VK_FORMAT_R8G8B8_UINT, &formatProp);

        if (vkCreateImage(app->GetVkDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(app->GetVkDevice(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = gfx::BufferHelper::FindMemoryType(app, memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(app->GetVkDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }
        //ͼ������ڴ�
        vkBindImageMemory(app->GetVkDevice(), image, imageMemory, 0);
    }
    static bool _HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
    static void _TransitionImageLayout(
        GFXVulkanApplication* app,
        VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        gfx::GFXCommandBufferScope commandBuffer(app);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (_HasStencilComponent(format))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            _GetVkCommandBuffer(commandBuffer),
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

    }
    static void _CopyBufferToImage(GFXVulkanApplication* app, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        gfx::GFXCommandBufferScope commandBuffer(app);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            _GetVkCommandBuffer(commandBuffer),
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    }

    GFXVulkanTexture2D::~GFXVulkanTexture2D()
    {
        if (m_inited)
        {
            vkDestroyImage(m_app->GetVkDevice(), m_textureImage, nullptr);
            vkFreeMemory(m_app->GetVkDevice(), m_textureImageMemory, nullptr);
        }
    }
    static VkFormat _GetVkFormat(int channel)
    {
        if (channel == 1)
        {
            return VkFormat::VK_FORMAT_R8_SRGB;
        }
        else if (channel == 2)
        {
            return VkFormat::VK_FORMAT_R8G8_SRGB;
        }
        else if (channel == 3)
        {
            return VkFormat::VK_FORMAT_R8G8B8_SRGB;
        }
        else if (channel == 4)
        {
            return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
        }
        assert(false);
        return {};
    }
    void GFXVulkanTexture2D::Init(
        GFXVulkanApplication* app,
        const uint8_t* imageData,
        int32_t width, int32_t height, int32_t channel,
        VkFormat format,
        bool enableReadWrite)
    {
        //��ȡͼƬ���ݴ�buffer
        //����ͼƬ��Դ
        //�任����Ѳ���
        //�ݴ�buffer������ͼ����Դ
        m_app = app;

        m_allowCpuRead = enableReadWrite;
        m_width = width;
        m_height = height;
        m_channel = channel;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        VkDeviceSize imageSize = width * height * channel;

        gfx::BufferHelper::CreateBuffer(app, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(app->GetVkDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, imageData, static_cast<size_t>(imageSize));
        vkUnmapMemory(app->GetVkDevice(), stagingBufferMemory);

        m_imageFormat = format;

        _CreateImage(app, width, height,
            m_imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

        _TransitionImageLayout(app, m_textureImage, m_imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        _CopyBufferToImage(app, stagingBuffer, m_textureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        //�任Ϊ�������
        _TransitionImageLayout(app, m_textureImage, m_imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(app->GetVkDevice(), stagingBuffer, nullptr);
        vkFreeMemory(app->GetVkDevice(), stagingBufferMemory, nullptr);

        m_inited = true;
    }
    const uint8_t* GFXVulkanTexture2D::GetData() const
    {
        return nullptr;
    }

    static void _ReleaseStbi(uint8_t* data)
    {
        stbi_image_free(data);
    }

    static void _CheckFormat(GFXVulkanApplication* app, VkFormat format)
    {
        VkFormatProperties prop;
        vkGetPhysicalDeviceFormatProperties(app->GetVkPhysicalDevice(), format, &prop);
        assert(prop.bufferFeatures);
    }

    static VkFormat _GetVkFormat(GFXTextureFormat format)
    {
        switch (format)
        {
        case gfx::GFXTextureFormat::R8: return VK_FORMAT_R8_UNORM;
        case gfx::GFXTextureFormat::R8G8B8: return VK_FORMAT_R8G8B8_UNORM;
        case gfx::GFXTextureFormat::R8G8B8A8: return VK_FORMAT_R8G8B8A8_UNORM;
        case gfx::GFXTextureFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        default:
            assert(false);
            break;
        }
        return {};
    }
    static std::vector<uint8_t> _FloatToByte(float* data, size_t len)
    {
        std::vector<uint8_t> newData(len);

        for (size_t i = 0; i < len; i++)
        {
            newData[i] = (uint8_t)(data[i] * 255);
        }
        return newData;
    }
    std::shared_ptr<GFXVulkanTexture2D> GFXVulkanTexture2D::CreateFromMemory(
        GFXVulkanApplication* app, const uint8_t* fileData, int32_t length, bool enableReadWrite, GFXTextureFormat format)
    {
        int x, y, channel;
        std::vector<uint8_t> buffer;

        switch (format)
        {
        case gfx::GFXTextureFormat::R8G8B8A8:
        {
            auto loaded = stbi_loadf_from_memory(fileData, length, &x, &y, &channel, 4);
            channel = 4;
            buffer = _FloatToByte(loaded, x * y * 4);
            stbi_image_free(loaded);
        }
        break;
        case gfx::GFXTextureFormat::R8G8B8A8_SRGB:
        {
            auto loaded = stbi_load_from_memory(fileData, length, &x, &y, &channel, 4);
            channel = 4;
            auto len = x * y * 4;
            buffer.assign(len, 0);
            memcpy(buffer.data(), loaded, len);
            stbi_image_free(loaded);
        }
        break;
        default:
            assert(false);
            break;
        }

        auto tex = new GFXVulkanTexture2D();
        tex->Init(app, buffer.data(), x, y, channel, _GetVkFormat(format), enableReadWrite);

        return std::shared_ptr<GFXVulkanTexture2D>(tex);
    }

}