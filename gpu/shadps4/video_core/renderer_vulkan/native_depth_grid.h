#pragma once
#include "common/types.h"
#include "native_depth_grid_policy.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include <functional>
namespace VideoCore {
class Image;
class BlitHelper;
} // namespace VideoCore
namespace Vulkan {
class Runtime;
class Instance;
class Scheduler;
} // namespace Vulkan
namespace Vulkan::NativeDepthGrid {
bool Enabled();
void Start(Runtime&, std::function<bool()>);
void Stop(Runtime&);
void NextFrame(Runtime&);
void NoteDraw(Runtime&, const VideoCore::Image&, bool main_scope, bool qualified, bool writes,
              bool clear, std::array<float, 2>, bool far_no_change);
void NativeWrite(Runtime&, const VideoCore::Image&, vk::AccessFlags2, vk::ImageLayout);
void AttachmentIntent(Runtime&, const VideoCore::Image*);
void Cleared(Runtime&, const VideoCore::Image&);
std::optional<std::array<float, 2>> ForCopy(Runtime&, const VideoCore::Image&,
                                            const VideoCore::Image&);
bool TryAlignedDepthCopy(const Instance&, Scheduler&, const VideoCore::BlitHelper*, u32, u32,
                         vk::Format, vk::Format, vk::Image, vk::Image, std::array<float, 2>);
void ReleaseHelper(const VideoCore::BlitHelper*);
} // namespace Vulkan::NativeDepthGrid
