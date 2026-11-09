#pragma once
#include <optional>
#include <functional>
#include "common/types.h"
#include "video_core/texture_cache/types.h"
#include "video_core/texture_cache/image_view.h"
#include "video_core/renderer_vulkan/vk_common.h"
namespace VideoCore { class TextureCache; struct Image; }
namespace Vulkan { class Runtime; }
namespace AmdGpu { class Liverpool; }
namespace Vulkan::SunAtlasProxy {
bool Enabled();
void Start(Runtime&, VideoCore::TextureCache&, std::function<bool()> owner_check, std::function<void()> final_drain);
void Stop(Runtime&);
// Call before any cache lock. A live lease pins Liverpool until synchronous completion.
void UnmapOnOwner(std::function<AmdGpu::Liverpool*()> dispatcher, bool on_recording_thread,
                  std::function<bool()> owner_check, std::function<void()> work);
// Shutdown brackets Liverpool destruction: Begin runs while its dispatcher is alive;
// Complete runs inside its destructor after join, before its fields die, then wakes guests.
void BeginOwnerShutdown(AmdGpu::Liverpool*);
void CompleteOwnerShutdown();
void NextFrame(Runtime&);
void BeginDraw(Runtime&, bool exact_sun_pair);
void Observe(Runtime&, VideoCore::ImageId);
struct Target { vk::ImageView view; vk::ImageLayout layout; u32 width, height; };
std::optional<Target> Attachment(Runtime&, VideoCore::ImageId);
std::optional<Target> Sample(Runtime&, VideoCore::ImageId, const VideoCore::ImageViewInfo&);
void NativeAccess(Runtime&, VideoCore::Image&, vk::AccessFlags2);
void Retire(Runtime&, VideoCore::Image&);
}
