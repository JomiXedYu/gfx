#include "GFXVulkanRenderPass.h"
#include "GFXVulkanApplication.h"
#include "BufferHelper.h"

namespace gfx
{
    GFXVulkanRenderPass::GFXVulkanRenderPass(GFXVulkanApplication* app)
        : m_app(app)
    {
        VkAttachmentDescription colorAttachment{};
        {
            colorAttachment.format = app->GetSwapChainImageFormat();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = BufferHelper::FindDepthFormat(app);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        {
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        VkAttachmentReference depthAttachmentRef{};
        {
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }


        VkSubpassDescription subpass{};
        {
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

        VkRenderPassCreateInfo renderPassInfo{};
        {
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 2;
            renderPassInfo.pAttachments = attachments;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;
        }

        if (vkCreateRenderPass(app->GetVkDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    GFXVulkanRenderPass::~GFXVulkanRenderPass()
    {
        vkDestroyRenderPass(m_app->GetVkDevice(), m_renderPass, nullptr);
    }
}