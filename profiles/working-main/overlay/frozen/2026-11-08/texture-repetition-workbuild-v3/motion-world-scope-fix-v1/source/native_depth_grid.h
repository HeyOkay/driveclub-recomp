#pragma once
#include "native_depth_grid_policy.h"
#include <functional>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
namespace VideoCore {class Image;class BlitHelper;}
namespace Vulkan {class Runtime;class Instance;class Scheduler;}
namespace Vulkan::NativeDepthGrid {
bool Enabled();bool DiagnosticsEnabled();
void Start(Runtime&,std::function<bool()>);void Stop(Runtime&);void NextFrame(Runtime&);
void NoteDraw(Runtime&,const VideoCore::Image&,bool main_scope,bool qualified,bool writes,bool clear,std::array<float,2>,std::uint64_t vertex_shader,std::uint32_t count,std::uint32_t predicates,bool far_no_change,std::uint64_t pixel_shader,std::uint32_t far_facts);
void NativeWrite(Runtime&,const VideoCore::Image&,vk::AccessFlags2,vk::ImageLayout);
void AttachmentIntent(Runtime&,const VideoCore::Image*);
void Copied(Runtime&,bool);
void ClearAttempt(Runtime&,const VideoCore::Image&,bool,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
void Cleared(Runtime&,const VideoCore::Image&,std::uint32_t clear_origin);
std::optional<std::array<float,2>> ForCopy(Runtime&,const VideoCore::Image&,const VideoCore::Image&);
bool TryAlignedDepthCopy(const Instance&,Scheduler&,const VideoCore::BlitHelper*,u32,u32,vk::Format,vk::Format,
                         vk::Image,vk::Image,std::array<float,2>);
void ReleaseHelper(const VideoCore::BlitHelper*);
}
