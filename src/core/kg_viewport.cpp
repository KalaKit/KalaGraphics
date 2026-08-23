//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <unordered_map>

#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"

#include "core/kg_viewport.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::ViewportStaticSize;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Resources::Shader;
using KalaGraphics::Resources::Camera;

using std::string_view;
using std::to_string;
using std::unordered_map;

//
// 4:3
//

static constexpr string_view vp_640_480   = "640x480";
static constexpr string_view vp_800_600   = "800x600";
static constexpr string_view vp_1024_768  = "1024x768";
static constexpr string_view vp_1600_1200 = "1600x1200";

//
// 16:9
//

static constexpr string_view vp_1280_720 = "1280x720";
static constexpr string_view vp_1600_900 = "1600x900";
static constexpr string_view vp_1920_1080 = "1920x1080";
static constexpr string_view vp_2560_1440 = "2560x1440";
static constexpr string_view vp_3840_2160 = "3840x2160";

//16:10

static constexpr string_view vp_1280_800 = "640x480";
static constexpr string_view vp_1680_1050 = "640x480";
static constexpr string_view vp_1920_1200 = "640x480";
static constexpr string_view vp_2560_1600 = "2560x1600";

//21:9

static constexpr string_view vp_2560_1080 = "2560x1080";
static constexpr string_view vp_3440_1440 = "3440x1440";
static constexpr string_view vp_5120_2160 = "5120x2160";

//32:9

static constexpr string_view vp_3840_1080 = "3840x1080";
static constexpr string_view vp_5120_1440 = "5120x1440";

static unordered_map<ViewportStaticSize, string_view, EnumHash<ViewportStaticSize>> vpNames =
{
    { ViewportStaticSize::VP_640_480,   vp_640_480 },
    { ViewportStaticSize::VP_800_600,   vp_800_600 },
    { ViewportStaticSize::VP_1024_768,  vp_1024_768 },
    { ViewportStaticSize::VP_1600_1200, vp_1600_1200 },

    { ViewportStaticSize::VP_1280_720,  vp_1280_720 },
    { ViewportStaticSize::VP_1600_900,  vp_1600_900 },
    { ViewportStaticSize::VP_1920_1080, vp_1920_1080 },
    { ViewportStaticSize::VP_2560_1440, vp_2560_1440 },
    { ViewportStaticSize::VP_3840_2160, vp_3840_2160 },

    { ViewportStaticSize::VP_1280_800,  vp_1280_800 },
    { ViewportStaticSize::VP_1680_1050, vp_1680_1050 },
    { ViewportStaticSize::VP_1920_1200, vp_1920_1200 },
    { ViewportStaticSize::VP_2560_1600, vp_2560_1600 },

    { ViewportStaticSize::VP_2560_1080, vp_2560_1080 },
    { ViewportStaticSize::VP_3440_1440, vp_3440_1440 },
    { ViewportStaticSize::VP_5120_2160, vp_5120_2160 },

    { ViewportStaticSize::VP_3840_1080, vp_3840_1080 },
    { ViewportStaticSize::VP_5120_1440, vp_5120_1440 }
};

static unordered_map<ViewportStaticSize, vec2, EnumHash<ViewportStaticSize>> vpSizes =
{
    { ViewportStaticSize::VP_640_480,   vec2(640, 480) },
    { ViewportStaticSize::VP_800_600,   vec2(800, 600) },
    { ViewportStaticSize::VP_1024_768,  vec2(1024, 768) },
    { ViewportStaticSize::VP_1600_1200, vec2(1600, 1200) },

    { ViewportStaticSize::VP_1280_720,  vec2(1280, 720) },
    { ViewportStaticSize::VP_1600_900,  vec2(1600, 900) },
    { ViewportStaticSize::VP_1920_1080, vec2(1920, 1080) },
    { ViewportStaticSize::VP_2560_1440, vec2(2560, 1440) },
    { ViewportStaticSize::VP_3840_2160, vec2(3840, 2160) },

    { ViewportStaticSize::VP_1280_800,  vec2(1280, 800) },
    { ViewportStaticSize::VP_1680_1050, vec2(1680, 1050) },
    { ViewportStaticSize::VP_1920_1200, vec2(1920, 1200) },
    { ViewportStaticSize::VP_2560_1600, vec2(2560, 1600) },

    { ViewportStaticSize::VP_2560_1080, vec2(2560, 1080) },
    { ViewportStaticSize::VP_3440_1440, vec2(3440, 1440) },
    { ViewportStaticSize::VP_5120_2160, vec2(5120, 2160) },

    { ViewportStaticSize::VP_3840_1080, vec2(3840, 1080) },
    { ViewportStaticSize::VP_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<Viewport> registry{};

    KalaGraphicsRegistry<Viewport>& Viewport::GetRegistry() { return registry; }

    string_view Viewport::GetViewportStaticName(ViewportStaticSize vpSize)
    {   
        string_view out{};
        string err = EnumToString(vpSize, vpNames, out);
        if (!err.empty())
        {
            Log::Print(
                "Failed to get viewport name! Reason: " + err, 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 Viewport::GetViewportStaticValue(ViewportStaticSize vpSize)
    {
		auto it = vpSizes.find(vpSize);
		if (it == vpSizes.end())
        {
            Log::Print(
                "Failed to get viewport value because the passed enum was invalid!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    Viewport* Viewport::Initialize(
        u32 contextID,
        ViewportType type,
        u32 targetViewport)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize viewport because its graphics context was invalid! Reason: " + err,
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Viewport> newVP = make_unique<Viewport>();
        Viewport* vpPtr = newVP.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        vpPtr->ID = newID;

        vpPtr->viewportType = type;
        vpPtr->targetViewportID = targetViewport;

        vpPtr->contextID = contextID;
        gctx->extraViewportIDs.push_back(newID);

        err = registry.AddContent(newID, std::move(newVP));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics viewport error",
				"Failed to initialize viewport! Reason: " + err);
        }

        return vpPtr;
    }
    Viewport* Viewport::_Initialize()
    {
        unique_ptr<Viewport> newVP = make_unique<Viewport>();
        Viewport* vpPtr = newVP.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        vpPtr->ID = newID;

        string err = registry.AddContent(newID, std::move(newVP));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics viewport error",
				"Failed to initialize viewport! Reason: " + err);
        }

        return vpPtr;
    }

    u32 Viewport::GetID() const { return ID; }

    u32 Viewport::GetContextID() const { return contextID; }

    u32 Viewport::GetPrimary3DCameraID() const { return primary3DCameraID; }
    u32 Viewport::GetPrimary2DCameraID() const { return primary2DCameraID; }

    const vector<u32>& Viewport::GetExtra3DCameraIDs() const { return extra3DCameraIDs; }
    const vector<u32>& Viewport::GetExtra2DCameraIDs() const { return extra2DCameraIDs; }

    u32 Viewport::GetPrimary3DShaderID() const { return primary3DShaderID; }
    u32 Viewport::GetPrimary2DShaderID() const { return primary2DShaderID; }

    const vector<u32>& Viewport::GetExtra3DShaderIDs() const { return extra3DShaderIDs; }
    const vector<u32>& Viewport::GetExtra2DShaderIDs() const { return extra2DShaderIDs; }

    u32 Viewport::GetTargetViewportID() const { return targetViewportID; }

    ViewportType Viewport::GetViewportType() const { return viewportType; }

    bool Viewport::IsRootViewport() const { return isRootViewport; }

    bool Viewport::IsStaticViewport() const { return isStaticViewport; }
    void Viewport::SetStaticViewportState(bool newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' static state because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' static state because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        isStaticViewport = newValue;

        string val = isStaticViewport ? "true" : "false";

        Log::Print(
            "Set viewport '" + to_string(contextID) + "' "
            "static state to " + val + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetViewportSize(bool isStatic) const
    {
        return isStatic 
            ? vpSizes[viewportStaticSize]
            : viewportDynamicSize;
    }
    void Viewport::SetViewportSize(ViewportStaticSize vpSize)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' static size because its graphics context was invalid! Reason: " + err);
        }

        vec2 realSize = GetViewportStaticValue(vpSize);
        if (realSize.x + viewportOffset.x > gctx->renderSize.x
            || realSize.y + viewportOffset.y > gctx->renderSize.y)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' static size because it is bigger than the window size!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        viewportStaticSize = vpSize;

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' static size to '" + string(GetViewportStaticName(viewportStaticSize)) + "'!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }
    void Viewport::SetViewportSize(vec2 newValue)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because its graphics context was invalid! Reason: " + err);
        }

        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0
            || newValue.y < 0)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (newValue.x + viewportOffset.x > gctx->renderSize.x
            || newValue.y + viewportOffset.y > gctx->renderSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it extended outside of render area!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        viewportDynamicSize = kclamp(
            newValue,
            0,
            gctx->renderSize - viewportOffset);

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' dynamic size to " 
            + to_string(viewportDynamicSize.x) + "x" 
            + to_string(viewportDynamicSize.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetViewportOffset() const { return viewportOffset; }
    void Viewport::SetViewportOffset(vec2 newValue)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because its graphics context was invalid! Reason: " + err);
        }

        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0.0f
            || newValue.y < 0.0f)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (newValue.x + viewportDynamicSize.x > gctx->renderSize.x
            || newValue.y + viewportDynamicSize.y > gctx->renderSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was extended outside of render area!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        viewportOffset = kclamp(
            newValue,
            0,
            gctx->renderSize - viewportDynamicSize);

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' offset to " 
            + to_string(viewportOffset.x) + "x" 
            + to_string(viewportOffset.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetScissorSize() const { return scissorSize; }
    void Viewport::SetScissorSize(vec2 newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isStaticViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is static!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0
            || newValue.y < 0)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor size because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (scissorOffset.x + newValue.x > viewportOffset.x + viewportDynamicSize.x
            || scissorOffset.y + newValue.y > viewportOffset.y + viewportDynamicSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor size because it extended outside of viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }
        
        scissorSize = kclamp(
            newValue, 
            0, 
            (viewportOffset + viewportDynamicSize) - scissorOffset);

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' scissor size to " 
            + to_string(scissorSize.x) + "x" 
            + to_string(scissorSize.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetScissorOffset() const { return scissorOffset; }
    void Viewport::SetScissorOffset(vec2 newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isStaticViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is static!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < viewportOffset.x
            || newValue.y < viewportOffset.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor offset because it was less than viewport offset!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }
        if (newValue.x + scissorSize.x > viewportOffset.x + viewportDynamicSize.x
            || newValue.y + scissorSize.y > viewportOffset.y + viewportDynamicSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor offset because it extended outside of viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        scissorOffset = kclamp(
            newValue, 
            viewportOffset, 
            (viewportOffset + viewportDynamicSize) - scissorSize);

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' scissor offset to " 
            + to_string(scissorOffset.x) + "x" 
            + to_string(scissorOffset.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    void Viewport::Update(u32 imageIndex)
    {
        GraphicsContext* gctx{};
        string _ = GraphicsContext::GetRegistry().GetContent(contextID, gctx);

        auto draw = [
            this,
            &gctx](bool is2D) -> void
            {
                VkViewport viewport{};
                viewport.x = viewportOffset.x;
                viewport.y = viewportOffset.y;

                if (isStaticViewport)
                {
                    vec2 size = GetViewportStaticValue(viewportStaticSize);
                    viewport.width = size.x;
                    viewport.height = size.y;
                }
                else
                {
                    viewport.width = viewportDynamicSize.x;
                    viewport.height = viewportDynamicSize.y;
                }

                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                VkRect2D scissor{};
                scissor.offset = 
                { 
                    scast<int>(scissorOffset.x), 
                    scast<int>(scissorOffset.y) 
                };
                scissor.extent = 
                { 
                    scast<u32>(scissorSize.x), 
                    scast<u32>(scissorSize.y) 
                };

                vkCmdSetViewport(
                    gctx->commandBuffers[gctx->currentFrame],
                    0,
                    1,
                    &viewport);
                    
                vkCmdSetScissor(
                    gctx->commandBuffers[gctx->currentFrame],
                    0,
                    1,
                    &scissor);

                if (!is2D)
                {
                    Shader* primary3DShader{};
                    string err = Shader::GetRegistry().GetContent(primary3DShaderID, primary3DShader);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics context error",
                            "Failed to update viewport '" + to_string(ID) 
                            + "' because the primary 3D shader was invalid! Reason: " + err);
                    }

                    primary3DShader->Update(gctx->commandBuffers[gctx->currentFrame]);

                    for (u32 extra3DShaderID : extra3DShaderIDs)
                    {
                        Shader* extra3DShader{};
                        err = Shader::GetRegistry().GetContent(extra3DShaderID, extra3DShader);
                        if (!err.empty())
                        {
                            KalaGraphicsCore::ForceClose(
                                "KalaGraphics context error",
                                "Failed to update viewport '" + to_string(ID) 
                                + "' because extra 3D shader was invalid! Reason: " + err);
                        }

                        extra3DShader->Update(gctx->commandBuffers[gctx->currentFrame]);
                    }
                }
                else
                {
                    Shader* primary2DShader{};
                    string err = Shader::GetRegistry().GetContent(primary2DShaderID, primary2DShader);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics context error",
                            "Failed to update viewport '" + to_string(ID) 
                            + "' because the primary 2D shader was invalid! Reason: " + err);
                    }

                    primary2DShader->Update(gctx->commandBuffers[gctx->currentFrame]);

                    for (u32 extra2DShaderID : extra2DShaderIDs)
                    {
                        Shader* extra2DShader{};
                        err = Shader::GetRegistry().GetContent(extra2DShaderID, extra2DShader);
                        if (!err.empty())
                        {
                            KalaGraphicsCore::ForceClose(
                                "KalaGraphics context error",
                                "Failed to update viewport '" + to_string(ID) 
                                + "' because extra 2D shader was invalid! Reason: " + err);
                        }

                        extra2DShader->Update(gctx->commandBuffers[gctx->currentFrame]);
                    }
                }
            };

        // ==================================================
        // 3D STAGE
        // ==================================================

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = gctx->swapchainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = { { 0.0f, 1.0f, 0.0f, 1.0f } };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = gctx->depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = { 0, 0 };
        renderingInfo.renderArea.extent = { scast<u32>(gctx->renderSize.x), scast<u32>(gctx->renderSize.y) };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(
            gctx->commandBuffers[gctx->currentFrame],
            &renderingInfo);

        draw(false);

        vkCmdEndRendering(gctx->commandBuffers[gctx->currentFrame]);

        // ==================================================
        // 2D STAGE
        // ==================================================

        VkRenderingAttachmentInfo colorAttachment2D{};
        colorAttachment2D.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment2D.imageView = gctx->swapchainImageViews[imageIndex];
        colorAttachment2D.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment2D.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment2D.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderingInfo2D{};
        renderingInfo2D.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo2D.renderArea.offset = { 0, 0 };
        renderingInfo2D.renderArea.extent = { scast<u32>(gctx->renderSize.x), scast<u32>(gctx->renderSize.y) };
        renderingInfo2D.layerCount = 1;
        renderingInfo2D.colorAttachmentCount = 1;
        renderingInfo2D.pColorAttachments = &colorAttachment2D;
        renderingInfo2D.pDepthAttachment = nullptr;

        vkCmdBeginRendering(
            gctx->commandBuffers[gctx->currentFrame],
            &renderingInfo2D);

        draw(true);

        vkCmdEndRendering(gctx->commandBuffers[gctx->currentFrame]);
    }

    void Viewport::Destroy()
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because it is a root viewport of graphics context '" + to_string(contextID) + "'!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        _Destroy();
    }

    void Viewport::_Destroy()
    {
		Log::Print(
			"Destroying viewport '" + to_string(ID) + "'.",
			"KG_VIEWPORT",
			LogType::LOG_INFO);

        if (viewportType == ViewportType::VP_OFFSCREEN)
        {
            Viewport* target{};
            string err = registry.GetContent(targetViewportID, target);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics viewport error",
                    "Failed to destroy viewport '" + to_string(ID) 
                    + "' because its target viewport was invalid! Reason: " + err);
            }
        }

        Shader* p3d{};
        string err = Shader::GetRegistry().GetContent(primary3DShaderID, p3d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy viewport '" + to_string(ID) + "' because "
                "its primary 3D shader was invalid! Reason: " + err);
        }

        p3d->isDestroyingViewport = true;
        p3d->Destroy();

        Shader* p2d{};
        err = Shader::GetRegistry().GetContent(primary2DShaderID, p2d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy viewport '" + to_string(ID) + "' because "
                "its primary 2D shader was invalid! Reason: " + err);
        }

        p3d->isDestroyingViewport = true;
        p3d->Destroy();

        for (u32 sID : extra3DShaderIDs)
        {
            Shader* s{};
            err = Shader::GetRegistry().GetContent(sID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 3D shader was invalid! Reason: " + err);
            }

            s->isDestroyingViewport = true;
            s->Destroy();
        }

        for (u32 sID : extra2DShaderIDs)
        {
            Shader* s{};
            err = Shader::GetRegistry().GetContent(sID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 2D shader was invalid! Reason: " + err);
            }

            s->isDestroyingViewport = true;
            s->Destroy();
        }

        Camera* pc3{};
        err = Camera::GetRegistry().GetContent(primary3DCameraID, pc3);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because its 3D camera was invalid! Reason: " + err);
        }

        pc3->Destroy();

        Camera* pc2{};
        err = Camera::GetRegistry().GetContent(primary2DCameraID, pc2);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because its 2D camera was invalid! Reason: " + err);
        }

        pc2->Destroy();

        for (u32 cID : extra3DCameraIDs)
        {
            Camera* c{};
            err = Camera::GetRegistry().GetContent(cID, c);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 3D camera was invalid! Reason: " + err);
            }

            c->Destroy();
        }

        for (u32 cID : extra2DCameraIDs)
        {
            Camera* c{};
            err = Camera::GetRegistry().GetContent(cID, c);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 2D camera was invalid! Reason: " + err);
            }

            c->Destroy();
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Viewport::~Viewport()
    {
		Log::Print(
			"Destroying graphics context '" + to_string(ID) + "'.",
			"KG_VIEWPORT",
			LogType::LOG_INFO);
    }
}