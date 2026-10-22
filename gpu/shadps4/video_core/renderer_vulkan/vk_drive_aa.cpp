// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_drive_aa.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "bbport_frame_info.h"
#include "bbport_settings.h"
#include "ffx_vk_portable.h"
#include "video_core/host_shaders/drive_aa_copy_comp.h"
#include "video_core/host_shaders/drive_aa_sharpen_comp.h"
#include "video_core/renderer_vulkan/vk_dlss.h"
#include "video_core/renderer_vulkan/vk_drive_motion.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/texture_cache.h"

namespace Vulkan {

namespace {

float Halton(u32 index, u32 base) {
    float f = 1.0f, r = 0.0f;
    while (index > 0) {
        f /= float(base);
        r += f * float(index % base);
        index /= base;
    }
    return r;
}

FfxVkPortableImage Describe(vk::Image image, vk::Format format, u32 width, u32 height,
                            vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
                            FfxVkPortableResourceState state) {
    FfxVkPortableImage out{};
    out.structSize = sizeof(out);
    out.image = image;
    out.format = static_cast<VkFormat>(format);
    out.extent = {width, height};
    out.mipCount = 1;
    out.arrayLayers = 1;
    out.usage = static_cast<VkImageUsageFlags>(usage);
    out.aspect = static_cast<VkImageAspectFlags>(aspect);
    out.state = state;
    return out;
}

void PrintIssues(const char* what, u64 issues) {
    std::printf("Anti-aliasing: %s invalid:", what);
    for (u32 bit = 0; bit < 64; ++bit) {
        if (issues & (1ull << bit)) {
            std::printf(" %s", ffxVkPortableValidationIssueName(1ull << bit));
        }
    }
    std::puts("");
}


} // namespace

DriveAA::DriveAA(const Instance& instance_, Scheduler& scheduler_,
                 VideoCore::TextureCache& texture_cache_, Runtime& runtime_, DriveMotion& motion_)
    : instance{instance_}, scheduler{scheduler_}, texture_cache{texture_cache_},
      runtime{runtime_}, motion{motion_} {}

DriveAA::~DriveAA() {
    if (context) {
        ffxVkPortableUpscaleContextDestroy(context);
    }
}

bool DriveAA::Active() const {
    return !failed && BbSettings::Get().aa_mode != BbSettings::AaOff;
}

// The render size is the output size over the scale (src/probe.c sets it at start).
bool DriveAA::Upscaling() const {
    return Active() && BbSettings::Get().aa_scale != BbSettings::AaScaleNative;
}

// The scale measured on the last run (render to output), else the chosen one.
float DriveAA::MipBias() const {
    if (!Upscaling()) {
        return 0.0f;
    }
    return -std::log2(scale > 1.0f ? scale : BbSettings::AaScaleFactor(BbSettings::Get().aa_scale));
}

void DriveAA::NextFrame() {
    const int mode = BbSettings::Get().aa_mode;
    if (mode != applied_mode) {
        applied_mode = mode;
        reset = true;
        failed = false;
        jitter_index = 0;
    }
    // Called for every draw into the display buffer: "running" means it ran lately.
    BbSettings::Get().aa_running =
        std::chrono::steady_clock::now() - last_run < std::chrono::milliseconds(300);
    if (!Active()) {
        jitter = {};
        return;
    }
    // 8 phases per 1:1 (16 for the native anti-aliasing), more as the scale grows.
    const float factor = Upscaling() && scale > 1.0f ? scale : 1.0f;
    const u32 phases = factor > 1.0f ? u32(std::ceil(8.0f * factor * factor)) : 16u;
    // The upscaled picture stands in for the game's during the one draw that scales it to the
    // output (OnComposite runs after this, for that draw).
    redirect_valid = false;
    jitter_index = jitter_index % phases + 1;
    jitter = {Halton(jitter_index, 2) - 0.5f, Halton(jitter_index, 3) - 0.5f};
    // The game's lighting and shadows rebuild positions from depth with the unshifted camera (it
    // does not jitter its own), so a shift of the geometry moves every lookup by as much: shadows
    // and the road mirage flickered when upscaling, where a render pixel covers several output
    // pixels. A shift of half an output pixel (render / output of a render pixel) errs no more than
    // at the render size, and still reaches the output pixels' centres. BB_AA_JITTER=<scale>: this
    // scale instead (0: none).
    static const float forced_scale = [] {
        const char* env = std::getenv("BB_AA_JITTER");
        return env && *env ? float(std::atof(env)) : -1.0f;
    }();
    const float jitter_scale = forced_scale >= 0.0f ? forced_scale : 1.0f / factor;
    jitter = {jitter[0] * jitter_scale, jitter[1] * jitter_scale};
}

vk::ImageView DriveAA::View(const VideoCore::Image& image, vk::Format format,
                            vk::ImageAspectFlags aspect) {
    const vk::Image handle = image.GetImage();
    CachedView* oldest = &views[0];
    for (auto& entry : views) {
        if (entry.view && entry.image == handle && entry.uid == image.image_uid &&
            entry.format == format && entry.aspect == aspect) {
            entry.last_use = ++view_uses;
            return entry.view;
        }
        if (entry.last_use < oldest->last_use) {
            oldest = &entry;
        }
    }
    const auto device = instance.GetDevice();
    if (oldest->view) {
        scheduler.DeferOperation([device, view = oldest->view] { device.destroyImageView(view); });
    }
    *oldest = {
        .image = handle,
        .uid = image.image_uid,
        .format = format,
        .aspect = aspect,
        .view = Check(device.createImageView({
            .image = handle,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {aspect, 0, 1, 0, 1},
        })),
        .last_use = ++view_uses,
    };
    return oldest->view;
}

void DriveAA::Redirect(const vk::ComponentMapping& mapping, vk::ImageView& view,
                       vk::ImageLayout& layout) {
    layout = vk::ImageLayout::eGeneral;
    if (mapping == vk::ComponentMapping{}) {
        view = *result_view;
        return;
    }
    for (const auto& [key, cached] : swizzled_views) {
        if (cached && key == mapping) {
            view = *cached;
            return;
        }
    }
    // Few swizzles read it (the game's passes agree on one); the oldest slot is reused.
    static u32 next = 0;
    auto& slot = swizzled_views[next++ % swizzled_views.size()];
    if (slot.second) {
        scheduler.DeferOperation([old = std::make_shared<vk::UniqueImageView>(std::move(slot.second))] {});
    }
    slot.first = mapping;
    slot.second = Check(instance.GetDevice().createImageViewUnique({
        .image = vk::Image(result_image),
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR16G16B16A16Sfloat,
        .components = mapping,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    }));
    view = *slot.second;
}

void DriveAA::CreatePipeline() {
    if (copy_pipeline) {
        return;
    }
    const auto device = instance.GetDevice();
    const std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {{
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
    }};
    copy_desc_layout = Check(device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    }));
    const vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                      .offset = 0,
                                      .size = 2 * sizeof(float)};
    copy_pipeline_layout = Check(device.createPipelineLayoutUnique({
        .setLayoutCount = 1,
        .pSetLayouts = &*copy_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    }));
    // Both take two storage images and up to two floats: the copy its size, the sharpening its
    // strength.
    const auto compute = [&](const auto& code) {
        const auto module = CompileSPV(code, device);
        auto pipeline = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                              .module = module,
                              .pName = "main"},
                    .layout = *copy_pipeline_layout,
                }));
        device.destroyShaderModule(module);
        return pipeline;
    };
    copy_pipeline = compute(DRIVE_AA_COPY_COMP);
    sharpen_pipeline = compute(DRIVE_AA_SHARPEN_COMP);
}

bool DriveAA::EnsureResources(u32 w, u32 h, u32 ow, u32 oh, bool dlss) {
    if (ready && w == width && h == height && ow == out_width && oh == out_height &&
        (context != nullptr) == !dlss) {
        return true;
    }
    scheduler.Finish();
    ready = false;
    for (auto& slot : swizzled_views) {
        slot.second.reset();
    }
    if (context) {
        ffxVkPortableUpscaleContextDestroy(context);
        context = nullptr;
    }
    width = w;
    height = h;
    out_width = ow;
    out_height = oh;
    if (!dlss) {
        FfxVkPortableDeviceInfo device_info{};
        device_info.structSize = sizeof(device_info);
        device_info.instance = instance.GetInstance();
        device_info.physicalDevice = instance.GetPhysicalDevice();
        device_info.device = instance.GetDevice();
        device_info.getDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
        device_info.queue = instance.GetGraphicsQueue();
        device_info.queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex();
        device_info.shaderFloat16Enabled = instance.IsShaderFloat16Enabled();
        device_info.subgroupSizeControlEnabled = instance.IsSubgroupSizeControlEnabled();
        device_info.synchronization2Enabled = VK_TRUE;
        device_info.shaderStorageImageWriteWithoutFormatEnabled = VK_TRUE;
        FfxVkPortableUpscaleCreateInfo create_info{};
        create_info.structSize = sizeof(create_info);
        // The game renders reversed Z with an infinite far plane.
        create_info.flags = FFX_VK_PORTABLE_CONTEXT_HDR_COLOR_INPUT |
                            FFX_VK_PORTABLE_CONTEXT_AUTO_EXPOSURE |
                            FFX_VK_PORTABLE_CONTEXT_DEPTH_INVERTED |
                            FFX_VK_PORTABLE_CONTEXT_DEPTH_INFINITE;
        create_info.maxRenderSize = {w, h};
        create_info.maxOutputSize = {ow, oh};
        if (const u64 issues = ffxVkPortableValidateUpscaleCreateInfo(&create_info)) {
            PrintIssues("create info", issues);
            return false;
        }
        if (ffxVkPortableUpscaleContextCreate(&device_info, &create_info, &context) !=
            FFX_VK_PORTABLE_OK) {
            std::puts("Anti-aliasing: FSR 3 context creation failed");
            context = nullptr;
            return false;
        }
    }
    const auto device = instance.GetDevice();
    const auto allocator = instance.GetAllocator();
    const auto make = [&](VideoCore::UniqueImage& image, vk::UniqueImageView& view,
                          vk::Format format, vk::ImageUsageFlags usage, u32 iw, u32 ih) {
        view.reset();
        image = VideoCore::UniqueImage(device, allocator);
        image.Create(vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {iw, ih, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        view = Check(device.createImageViewUnique({
            .image = vk::Image(image),
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        }));
    };
    make(motion_image, motion_view, vk::Format::eR16G16Sfloat,
         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled, w, h);
    make(result_image, result_view, vk::Format::eR16G16B16A16Sfloat,
         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
             vk::ImageUsageFlagBits::eTransferSrc,
         ow, oh);
    if (dlss) {
        make(raw_image, raw_view, vk::Format::eR16G16B16A16Sfloat,
             vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled, ow, oh);
    }
    CreatePipeline();
    ready = true;
    reset = true;
    if (ow == w && oh == h) {
        std::printf("Anti-aliasing: %s %ux%u", dlss ? "DLAA" : "FSR 3.1 native", w, h);
    } else {
        std::printf("Upscaling: %s %ux%u to %ux%u", dlss ? "DLSS" : "FSR 3.1", w, h, ow, oh);
    }
    std::puts("");
    return true;
}

void DriveAA::OnTaa(VideoCore::ImageId color_id, VideoCore::ImageId depth_id,
                    VideoCore::ImageId output_id) {
    static int reported = 0;
    const auto skip = [&](const char* why) {
        reset = true;
        if (reported++ < 6) {
            std::printf("Anti-aliasing: skipped, %s", why);
            std::puts("");
        }
    };
    late_pending = false;
    redirect_valid = false;
    if (!Active() || !color_id || !depth_id || !output_id) {
        return skip("no image");
    }
    if (!motion.Ready()) {
        return skip("the game camera is not known yet");
    }
    auto& color = texture_cache.GetImage(color_id);
    auto& depth = texture_cache.GetImage(depth_id);
    auto& output = texture_cache.GetImage(output_id);
    const u32 w = color.info.size.width, h = color.info.size.height;
    if (depth.info.size.width != w || depth.info.size.height != h || output.info.size.width != w ||
        output.info.size.height != h || !(output.usage_flags & vk::ImageUsageFlagBits::eStorage)) {
        std::printf("Anti-aliasing: sizes color %ux%u depth %ux%u output %ux%u storage %d", w, h,
                    depth.info.size.width, depth.info.size.height, output.info.size.width,
                    output.info.size.height, int(bool(output.usage_flags & vk::ImageUsageFlagBits::eStorage)));
        std::puts("");
        return skip("image sizes or usage differ");
    }
    const u32 display_w = BbFrame::output_w.load(std::memory_order_relaxed);
    const u32 display_h = BbFrame::output_h.load(std::memory_order_relaxed);
    if (!Upscaling() || display_w <= w || display_h <= h) {
        // At the render size: the reconstructed picture replaces the pass's result.
        Reconstruct(color_id, depth_id, output_id, jitter, false);
        return;
    }
    // Upscaling: the passes after this one (transparents, bloom, motion blur and depth of field)
    // draw at the render size, and the picture only reaches the output size when the game scales
    // it into its display buffer. So the upscale waits for that pass (OnComposite), and the
    // game's own anti-aliasing is skipped here: its result is the lit picture as it is, still
    // aliased and jittered, as FSR and DLSS want their input.
    scheduler.EndRendering();
    runtime.Transit(&color, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead);
    runtime.Transit(&output, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite);
    runtime.FlushBarriers();
    const auto cmdbuf = scheduler.CommandBuffer();
    const vk::ImageCopy region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = {w, h, 1},
    };
    cmdbuf.copyImage(color.GetImage(), vk::ImageLayout::eGeneral, output.GetImage(),
                     vk::ImageLayout::eGeneral, region);
    const vk::MemoryBarrier2 copied{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &copied});
    late_pending = true;
    late_depth = depth_id;
    late_jitter = jitter;  // the jitter the scene was drawn with (NextFrame moves on before the composite)
    late_width = w;
    late_height = h;
}

bool DriveAA::WantsComposite(const VideoCore::Image& image) const {
    return late_pending && image.info.size.width == late_width &&
           image.info.size.height == late_height &&
           image.info.pixel_format == vk::Format::eB10G11R11UfloatPack32;
}

void DriveAA::OnComposite(VideoCore::ImageId color_id) {
    late_pending = false;
    static int reported = 0;
    if (reported++ < 2) {
        const auto& color = texture_cache.GetImage(color_id);
        std::printf("Upscaling: the game's picture %#llx (%ux%u) upscaled where it is scaled to the output\n",
                    static_cast<unsigned long long>(color.info.guest_address), color.info.size.width,
                    color.info.size.height);
    }
    Reconstruct(color_id, late_depth, {}, late_jitter, true);
}

void DriveAA::Reconstruct(VideoCore::ImageId color_id, VideoCore::ImageId depth_id,
                          VideoCore::ImageId output_id, std::array<float, 2> frame_jitter,
                          bool upscale) {
    auto& color = texture_cache.GetImage(color_id);
    auto& depth = texture_cache.GetImage(depth_id);
    const u32 w = color.info.size.width, h = color.info.size.height;
    if (depth.info.size.width != w || depth.info.size.height != h) {
        reset = true;
        return;
    }
    const bool want_dlss = BbSettings::Get().aa_mode == BbSettings::AaDlss;
    Dlss* dlss = want_dlss ? Dlss::Get() : nullptr;
    if (want_dlss && (!dlss || !dlss->Available())) {
        std::puts("Anti-aliasing: DLSS is not available; using FSR 3.1");
        BbSettings::Get().aa_mode = BbSettings::AaFsr3;
        reset = true;
        return;
    }
    // Upscaled to the game's output size (its display buffer).
    const u32 ow = upscale ? BbFrame::output_w.load(std::memory_order_relaxed) : w;
    const u32 oh = upscale ? BbFrame::output_h.load(std::memory_order_relaxed) : h;
    const float factor = float(ow) / float(w);
    scale = factor;
    if (!EnsureResources(w, h, ow, oh, want_dlss)) {
        failed = true;
        return;
    }
    const auto color_format = color.info.pixel_format;
    const auto depth_format = depth.info.pixel_format;
    const auto color_view = View(color, color_format, vk::ImageAspectFlagBits::eColor);
    const auto depth_view = View(depth, depth_format, vk::ImageAspectFlagBits::eDepth);

    // A new size or model makes a new DLSS feature. The old one's images and buffers may still be
    // used by recorded or submitted work (this command buffer can already hold an evaluation), so
    // all of it finishes on the GPU before they are released; then this command buffer starts
    // afresh. WaitSubmitted released them while the GPU still wrote to them: a device loss
    // (WriteInvalid) when leaving a race, where the sizes change.
    const Dlss::FeatureDesc dlss_desc{w, h, ow, oh, upscale ? Dlss::QualityForScale(factor) : 0, true, true,
                                      u32(BbSettings::Get().dlss_model.load())};
    const bool new_feature = dlss && !dlss->HasFeature(dlss_desc);
    if (new_feature) {
        scheduler.Finish();
        dlss->ReleaseFeature();
    }

    scheduler.EndRendering();
    runtime.Transit(&depth, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead);
    runtime.Transit(&color, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead);
    VideoCore::Image* output = output_id ? &texture_cache.GetImage(output_id) : nullptr;
    if (output) {
        runtime.Transit(output, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                        vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
    }
    runtime.FlushBarriers();
    const auto cmdbuf = scheduler.CommandBuffer();

    const auto all = vk::PipelineStageFlagBits2::eAllCommands;
    const auto rw = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
    const auto barrier = [&](vk::Image image, vk::ImageLayout old_layout,
                             vk::PipelineStageFlags2 src_stage, vk::AccessFlags2 src_access,
                             vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access) {
        const vk::ImageMemoryBarrier2 b{
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = old_layout,
            .newLayout = vk::ImageLayout::eGeneral,
            .image = image,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        };
        cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b});
    };
    barrier(vk::Image(motion_image), vk::ImageLayout::eUndefined, all, vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite);
    barrier(vk::Image(result_image), vk::ImageLayout::eUndefined, all, vk::AccessFlagBits2::eNone,
            all, rw);

    // Camera motion in render pixels (previous minus current).
    // No stencil here: the anti-aliasing keeps the camera's vectors for every pixel, as before.
    motion.Record(cmdbuf, depth_view, depth_view, *motion_view, w, h, w, h, false);
    barrier(vk::Image(motion_image), vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, all,
            vk::AccessFlagBits2::eShaderRead);

    const auto now = std::chrono::steady_clock::now();
    float frame_ms = std::chrono::duration<float, std::milli>(now - last_frame).count();
    if (!(frame_ms > 0.0f && frame_ms < 200.0f)) {
        frame_ms = 16.6f;
    }
    last_frame = now;

    bool dispatched = false;
    const auto& settings = BbSettings::Get();
    const float sharpness = settings.sharpen ? std::min(settings.sharpness.load(), 1.0f) : 0.0f;
    // DLSS ignores its sharpness since 2.5.1: RCAS sharpens its result afterwards (FSR has its own).
    const bool sharpen_after = dlss && sharpness > 0.0f && raw_image;
    if (sharpen_after) {
        barrier(vk::Image(raw_image), vk::ImageLayout::eUndefined, all, vk::AccessFlagBits2::eNone,
                all, rw);
    }
    if (dlss) {
        bool ok = true;
        if (new_feature) {
            ok = dlss->CreateFeature(cmdbuf, dlss_desc);
            reset = true;
        }
        if (ok) {
            ok = dlss->Evaluate(
                cmdbuf,
                {.color = {color.GetImage(), color_view, color_format,
                           vk::ImageAspectFlagBits::eColor, w, h},
                 .depth = {depth.GetImage(), depth_view, depth_format,
                           vk::ImageAspectFlagBits::eDepth, w, h},
                 .motion = {vk::Image(motion_image), *motion_view, vk::Format::eR16G16Sfloat,
                            vk::ImageAspectFlagBits::eColor, w, h},
                 .output = {sharpen_after ? vk::Image(raw_image) : vk::Image(result_image),
                            sharpen_after ? *raw_view : *result_view, vk::Format::eR16G16B16A16Sfloat,
                            vk::ImageAspectFlagBits::eColor, ow, oh},
                 .jitter_x = frame_jitter[0],
                 .jitter_y = frame_jitter[1],
                 .reset = reset,
                 .frame_ms = frame_ms,
                 .sharpness = sharpness});
        }
        if (!ok) {
            std::puts("Anti-aliasing: DLSS failed; using FSR 3.1");
            BbSettings::Get().aa_mode = BbSettings::AaFsr3;
            reset = true;
            return;
        }
        dispatched = true;
    } else {
        FfxVkPortableUpscaleDispatchInfo info{};
        info.structSize = sizeof(info);
        info.commandBuffer = cmdbuf;
        info.color = Describe(color.GetImage(), color_format, w, h, color.usage_flags,
                              vk::ImageAspectFlagBits::eColor,
                              FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
        info.depth = Describe(depth.GetImage(), depth_format, w, h, depth.usage_flags,
                              vk::ImageAspectFlagBits::eDepth,
                              FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
        info.motionVectors =
            Describe(vk::Image(motion_image), vk::Format::eR16G16Sfloat, w, h,
                     vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                     vk::ImageAspectFlagBits::eColor, FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
        info.output = Describe(vk::Image(result_image), vk::Format::eR16G16B16A16Sfloat, ow, oh,
                               vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eTransferSrc,
                               vk::ImageAspectFlagBits::eColor,
                               FFX_VK_PORTABLE_RESOURCE_STATE_UNORDERED_ACCESS);
        info.exposure.structSize = sizeof(info.exposure);
        info.reactiveMask.structSize = sizeof(info.reactiveMask);
        info.transparencyAndCompositionMask.structSize =
            sizeof(info.transparencyAndCompositionMask);
        info.jitterOffset = {frame_jitter[0], frame_jitter[1]};
        info.motionVectorScale = {1.0f, 1.0f};
        info.renderSize = {w, h};
        info.outputSize = {ow, oh};
        info.frameTimeMilliseconds = frame_ms;
        info.preExposure = 1.0f;
        info.cameraNear = motion.Near();
        info.cameraFar = 3000.0f;
        info.cameraVerticalFovRadians = motion.VerticalFov();
        info.viewSpaceToMeters = 1.0f;
        info.sharpness = sharpness;
        info.enableSharpening = settings.sharpen ? VK_TRUE : VK_FALSE;
        info.reset = reset ? VK_TRUE : VK_FALSE;
        info.frameId = frame_id++;
        FfxVkPortableUpscaleCreateInfo create_info{};
        create_info.structSize = sizeof(create_info);
        create_info.flags = FFX_VK_PORTABLE_CONTEXT_HDR_COLOR_INPUT |
                            FFX_VK_PORTABLE_CONTEXT_AUTO_EXPOSURE |
                            FFX_VK_PORTABLE_CONTEXT_DEPTH_INVERTED |
                            FFX_VK_PORTABLE_CONTEXT_DEPTH_INFINITE;
        create_info.maxRenderSize = {w, h};
        create_info.maxOutputSize = {ow, oh};
        if (const u64 issues = ffxVkPortableValidateUpscaleDispatchInfo(&create_info, &info)) {
            PrintIssues("dispatch", issues);
            failed = true;
            return;
        }
        if (ffxVkPortableUpscaleContextRecordDispatch(context, &info) != FFX_VK_PORTABLE_OK) {
            std::puts("Anti-aliasing: FSR 3 dispatch failed");
            failed = true;
            return;
        }
        dispatched = true;
    }
    if (!dispatched) {
        return;
    }
    reset = false;
    last_run = std::chrono::steady_clock::now();
    BbSettings::Get().aa_running = true;

    if (sharpen_after) {
        barrier(vk::Image(raw_image), vk::ImageLayout::eGeneral, all, rw,
                vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);
        const vk::DescriptorImageInfo raw_info{.imageView = *raw_view,
                                               .imageLayout = vk::ImageLayout::eGeneral};
        const vk::DescriptorImageInfo result_info{.imageView = *result_view,
                                                  .imageLayout = vk::ImageLayout::eGeneral};
        const std::array<vk::WriteDescriptorSet, 2> writes = {{
            {.dstBinding = 0,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eStorageImage,
             .pImageInfo = &raw_info},
            {.dstBinding = 1,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eStorageImage,
             .pImageInfo = &result_info},
        }};
        const std::array<float, 2> strength{sharpness, 0.0f};
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *sharpen_pipeline);
        cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *copy_pipeline_layout, 0, writes);
        cmdbuf.pushConstants(*copy_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(strength), strength.data());
        cmdbuf.dispatch((ow + 7) / 8, (oh + 7) / 8, 1);
        barrier(vk::Image(result_image), vk::ImageLayout::eGeneral,
                vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, all, rw);
    }

    if (upscale) {
        // The pass scaling the game's picture into its display buffer samples the upscaled one
        // instead (by UV, so the size does not matter); what reads the picture by pixel keeps
        // the game's.
        const vk::MemoryBarrier2 ready_to_read{
            .srcStageMask = all,
            .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
            .dstStageMask = all,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        };
        cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &ready_to_read});
        redirect_id = color_id;
        redirect_valid = true;
        return;
    }
    if (!output) {
        return;
    }
    // The reconstructed picture replaces what the game's pass wrote.
    const auto out_view = View(*output, vk::Format::eB10G11R11UfloatPack32,
                               vk::ImageAspectFlagBits::eColor);
    barrier(vk::Image(result_image), vk::ImageLayout::eGeneral, all, rw,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);
    const vk::DescriptorImageInfo src_info{.imageView = *result_view,
                                           .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo dst_info{.imageView = out_view,
                                           .imageLayout = vk::ImageLayout::eGeneral};
    const std::array<vk::WriteDescriptorSet, 2> writes = {{
        {.dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &src_info},
        {.dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &dst_info},
    }};
    const std::array<float, 2> size{float(w), float(h)};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *copy_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *copy_pipeline_layout, 0, writes);
    cmdbuf.pushConstants(*copy_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(size),
                         size.data());
    cmdbuf.dispatch((w + 7) / 8, (h + 7) / 8, 1);
    const vk::MemoryBarrier2 done{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &done});
}

} // namespace Vulkan
