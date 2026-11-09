#include "sun_atlas_proxy.h"
#include "sun_atlas_proxy_2x_policy.h"
#include "sun_atlas_proxy_sync_policy.h"
#include "sun_atlas_proxy_alias_policy.h"
#include "sun_atlas_proxy_policy.h"
#include "sun_atlas_proxy_owner_gate.h"
#include "video_core/amdgpu/liverpool.h"
#include <atomic>
#include <chrono>
#include <cerrno>
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/texture_cache.h"
#include <vk_mem_alloc.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
namespace Vulkan::SunAtlasProxy {
namespace {
constexpr auto Aspects=vk::ImageAspectFlagBits::eDepth|vk::ImageAspectFlagBits::eStencil;
constexpr auto Writes=vk::AccessFlagBits2::eShaderWrite|vk::AccessFlagBits2::eTransferWrite|
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite|vk::AccessFlagBits2::eColorAttachmentWrite|
    vk::AccessFlagBits2::eMemoryWrite;
AliasIdentity Identity(const VideoCore::Image& i) {
    return {i.image_uid,i.backing ? reinterpret_cast<std::uintptr_t>(static_cast<VkImage>(i.GetImage())) : 0,
        reinterpret_cast<std::uintptr_t>(i.backing),
        {i.info.guest_address,i.info.guest_size},ActiveStencil(i.info.props.has_stencil,{i.info.stencil_addr,i.info.stencil_size})};
}
struct FamilySnapshot { Runtime* runtime; AliasIdentity identity; u64 uid; VAddr address,stencil; u64 bytes,stencil_bytes; };
std::shared_ptr<const FamilySnapshot> family_snapshot;
void PublishSnapshot(std::shared_ptr<const FamilySnapshot> next,std::memory_order order) {
    std::atomic_store_explicit(&family_snapshot,std::move(next),order);
}
bool MayBelong(Runtime& r,const VideoCore::Image& i) {
    const auto f=std::atomic_load_explicit(&family_snapshot,std::memory_order_acquire);
    if(!f || f->runtime!=&r) return false;
    const u64 b=i.info.guest_address,e=b+i.info.guest_size;
    return i.image_uid==f->uid || (b<f->address+f->bytes && f->address<e) ||
        (f->stencil && b<f->stencil+f->stencil_bytes && f->stencil<e);
}
struct Entry {
    vk::Device device; VmaAllocator allocator{}; VkImage image{}; VmaAllocation allocation{};
    VideoCore::ImageId id{}; u64 uid{}; vk::Image native{}; const void* native_backing{};
    VAddr address{}, stencil{}; u64 bytes{}, stencil_bytes{};
    u32 width{},height{},native_width{},native_height{};
    u64 allocation_bytes{}; // diagnostics: VmaAllocationInfo.size
    vk::ImageLayout layout=vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 last_stage{}; vk::AccessFlags2 last_access{};
    vk::UniqueImageView attachment;
    std::vector<std::pair<VideoCore::ImageViewInfo,vk::UniqueImageView>> views;
    Coherence state;
    ~Entry() { views.clear(); attachment.reset(); if(image) vmaDestroyImage(allocator,image,allocation); }
};
struct State {
    Runtime& runtime; VideoCore::TextureCache& cache;
    std::unique_ptr<Entry> entry;
    VideoCore::ImageId candidate{}; u64 uid{},frame{},seen{};
    bool exact_pair=false, copying=false, failed=false;
    bool active_scaled_clip_window=false;
    SyncPolicy sync;
    u64 write_barriers{},write_tentative{},write_coalesced{},write_repairs{},read_merges{};
    std::optional<bool> scaled_clip_window_supported;
    unsigned logs=0;
    struct Diagnostics {
        u64 uid{},address{},seen_frame{},attachments{},last_attachment_frame{},samples{},last_sample_frame{};
        u64 resolves{},last_resolve_frame{},inits{},last_init_frame{},native_writes{},invalidations{},last_invalidation_frame{};
        u64 allocations{},allocation_bytes{},summary_frame{};
        bool summarized=false;
        bool clip_disabled_fallback=false;
    } diag;
    u64 previous_frame{};
    void Summary(const char* reason,bool periodic=false) {
        std::printf("BB_SUN_PROXY_SYNC: reason=%s frame=%llu write_barriers=%llu tentative=%llu coalesced=%llu repairs=%llu read_merges=%llu\n",reason,
            (unsigned long long)frame,(unsigned long long)write_barriers,(unsigned long long)write_tentative,
            (unsigned long long)write_coalesced,(unsigned long long)write_repairs,(unsigned long long)read_merges);std::fflush(stdout);
        if(!periodic) {
            if(diag.summarized && diag.summary_frame==frame) return;
            diag.summarized=true;diag.summary_frame=frame;
        }
        // Independent of Log's 64-event budget. A later Stop reports its current
        // frame even if the last family retired earlier; same-frame Drop/Stop deduplicates.
        std::printf("BB_SUN_ATLAS_PROXY_SUMMARY: reason=%s uid=%llu guest=0x%llx seen_frame=%llu current_frame=%llu previous_frame=%llu attachments=%llu last_attachment_frame=%llu samples=%llu last_sample_frame=%llu resolves=%llu last_resolve_frame=%llu inits=%llu last_init_frame=%llu native_writes=%llu invalidations=%llu last_invalidation_frame=%llu allocations=%llu allocation_bytes=%llu live_allocation_bytes=%llu failed=%u clip_disabled_fallback=%u scale=%u\n",
            reason,(unsigned long long)diag.uid,(unsigned long long)diag.address,
            (unsigned long long)diag.seen_frame,(unsigned long long)frame,(unsigned long long)previous_frame,
            (unsigned long long)diag.attachments,(unsigned long long)diag.last_attachment_frame,
            (unsigned long long)diag.samples,(unsigned long long)diag.last_sample_frame,
            (unsigned long long)diag.resolves,(unsigned long long)diag.last_resolve_frame,
            (unsigned long long)diag.inits,(unsigned long long)diag.last_init_frame,
            (unsigned long long)diag.native_writes,(unsigned long long)diag.invalidations,
            (unsigned long long)diag.last_invalidation_frame,(unsigned long long)diag.allocations,
            (unsigned long long)diag.allocation_bytes,(unsigned long long)(entry?entry->allocation_bytes:0),unsigned(failed),unsigned(diag.clip_disabled_fallback),Scale());
        std::fflush(stdout);
    }
    unsigned periodic_summaries{};
    bool periodic_started=false;
    std::chrono::steady_clock::time_point next_periodic{};
    static long long SummaryIntervalMs() {
        static const long long interval=[] {
            const char* p=std::getenv("BB_SUN_PROXY_SUMMARY_INTERVAL_MS");
            if(!p || !*p) return 0ll;
            char* end=nullptr;errno=0;const auto ms=std::strtoll(p,&end,10);
            return errno || end==p || *end || ms<=0 || ms>86400000 ? 0ll : ms;
        }();
        return interval;
    }
    void PeriodicSummaryAt(std::chrono::steady_clock::time_point now) {
        const auto ms=SummaryIntervalMs();
        if(!ms || periodic_summaries>=64) return;
        const auto interval=std::chrono::milliseconds{ms};
        if(!periodic_started) { periodic_started=true;next_periodic=now+interval;return; }
        if(now<next_periodic) return;
        ++periodic_summaries;next_periodic=now+interval;
        Summary("periodic",true); // never consumes terminal-summary dedup state
    }
    void NoteSample() { ++diag.samples;diag.last_sample_frame=frame; }
    void Log(const char* text) { if(logs++<64) { std::printf("BB_SUN_ATLAS_PROXY: %s frame=%llu uid=%llu scale=%u\n",text,(unsigned long long)frame,(unsigned long long)uid,Scale()); std::fflush(stdout); } }
    VideoCore::Image* Original() { return cache.TryGetImage(candidate,uid); }
    bool Matches(const VideoCore::Image& i) const {
        if(!entry) return false;
        if(i.image_uid==entry->uid || i.GetImage()==entry->native) return true;
        const u64 b=i.info.guest_address,e=b+i.info.guest_size;
        return (b<entry->address+entry->bytes && entry->address<e) ||
               (entry->stencil && b<entry->stencil+entry->stencil_bytes && entry->stencil<e);
    }
    void Transition(vk::ImageLayout next, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) {
        auto& e=*entry; auto& s=runtime.GetScheduler();
        constexpr auto sample_stages=vk::PipelineStageFlagBits2::eAllGraphics|vk::PipelineStageFlagBits2::eComputeShader;
        const bool sample_request=next==vk::ImageLayout::eDepthStencilReadOnlyOptimal &&
            access==vk::AccessFlagBits2::eShaderSampledRead && stage==sample_stages;
        if(SyncPolicy::MergeRead(sample_request,e.layout==next,e.last_access==access,
                                 !(stage & ~e.last_stage))) {++read_merges;return;}
        s.EndRendering();
        // Include an explicit WAW/RAW dependency even when the layout stays unchanged.
        const vk::ImageMemoryBarrier2 barrier{
            .srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask=vk::AccessFlagBits2::eMemoryRead|vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask=stage,.dstAccessMask=access,.oldLayout=e.layout,.newLayout=next,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=vk::Image(e.image),.subresourceRange={Aspects,0,1,0,1}};
        s.Record([barrier](vk::CommandBuffer cmd){cmd.pipelineBarrier2({.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier});});
        e.layout=next;e.last_stage=stage;e.last_access=access;
        if(next==vk::ImageLayout::eDepthStencilAttachmentOptimal) ++write_barriers;
    }
    void Copy(bool to_native) {
        auto* original=Original();
        // Retirement hooks resolve before a UID/backing changes. A mismatch must
        // never silently publish stale native pixels.
        if(!original || original->GetImage()!=entry->native || original->backing!=entry->native_backing) { std::abort(); }
        copying=true; auto& e=*entry; auto& s=runtime.GetScheduler(); s.EndRendering();
        runtime.Transit(original,to_native?vk::ImageLayout::eTransferDstOptimal:vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eTransfer,to_native?vk::AccessFlagBits2::eTransferWrite:vk::AccessFlagBits2::eTransferRead);
        runtime.FlushBarriers();
        Transition(to_native?vk::ImageLayout::eTransferSrcOptimal:vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits2::eTransfer,to_native?vk::AccessFlagBits2::eTransferRead:vk::AccessFlagBits2::eTransferWrite);
        const auto src=to_native?vk::Image(e.image):original->GetImage();
        const auto dst=to_native?original->GetImage():vk::Image(e.image);
        const auto geometry=CopyGeometry({e.native_width,e.native_height},{e.width,e.height},to_native);
        for(auto aspect:{vk::ImageAspectFlagBits::eDepth,vk::ImageAspectFlagBits::eStencil}) {
            const vk::ImageBlit region{.srcSubresource={aspect,0,0,1},
                .srcOffsets=std::array{vk::Offset3D{},vk::Offset3D{s32(geometry.src.width),s32(geometry.src.height),1}},
                .dstSubresource={aspect,0,0,1},
                .dstOffsets=std::array{vk::Offset3D{},vk::Offset3D{s32(geometry.dst.width),s32(geometry.dst.height),1}}};
            s.Record([src,dst,region](vk::CommandBuffer cmd){cmd.blitImage(src,vk::ImageLayout::eTransferSrcOptimal,dst,vk::ImageLayout::eTransferDstOptimal,region,vk::Filter::eNearest);});
        }
        if(to_native) { ++diag.resolves;diag.last_resolve_frame=frame; e.state.Resolved(); original->flags|=VideoCore::ImageFlagBits::GpuModified; Log("resolve-depth-and-stencil"); }
        else { ++diag.inits;diag.last_init_frame=frame; e.state.Filled(); Log("initialize-depth-and-stencil"); }
        copying=false;
    }
    void Drop() {
        if(!entry) return;
        if(entry->state.Dirty()) Copy(true);
        Summary("retire");
        runtime.GetScheduler().EndRendering();
        runtime.GetScheduler().DeferOperation([old=std::move(entry)]{});
        PublishSnapshot({},std::memory_order_release);
        candidate={};uid=0; Log("retire");
    }
    bool Ensure() {
        auto* i=Original();
        if(!i || !uid || frame<=seen || failed) return false;
        if(entry && (entry->native!=i->GetImage() || entry->native_backing!=i->backing)) { Drop(); return false; }
        if(!entry) {
            auto e=std::make_unique<Entry>();
            const auto& instance=runtime.GetInstance(); e->device=instance.GetDevice();e->allocator=instance.GetAllocator();
            e->id=candidate;e->uid=uid;e->native=i->GetImage();e->native_backing=i->backing;e->address=i->info.guest_address;e->bytes=i->info.guest_size;e->stencil=i->info.stencil_addr;e->stencil_bytes=i->info.stencil_size;
            e->native_width=i->info.size.width;e->native_height=i->info.size.height;
            const auto host=HostSize({e->native_width,e->native_height},Scale());e->width=host.width;e->height=host.height;
            // No pool fallback and no host-memory/over-budget retry.
            auto ci=i->backing->image.image_ci;ci.pNext=nullptr;ci.extent=vk::Extent3D{e->width,e->height,1};
            ci.usage=vk::ImageUsageFlagBits::eDepthStencilAttachment|vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst;
            const auto props=instance.GetPhysicalDevice().getFormatProperties(vk::Format::eD32SfloatS8Uint).optimalTilingFeatures;
            const auto need=vk::FormatFeatureFlagBits::eBlitSrc|vk::FormatFeatureFlagBits::eBlitDst|vk::FormatFeatureFlagBits::eDepthStencilAttachment|vk::FormatFeatureFlagBits::eSampledImage;
            if((props&need)!=need) { failed=true;Log("unsupported-format-native-fallback");return false; }
            const auto limits=instance.GetPhysicalDevice().getProperties().limits;
            const auto format=instance.GetPhysicalDevice().getImageFormatProperties(ci.format,ci.imageType,ci.tiling,ci.usage,ci.flags);
            if(format.result!=vk::Result::eSuccess || !DimensionsSupported(host,limits.maxImageDimension2D,limits.maxFramebufferWidth,limits.maxFramebufferHeight,limits.maxViewportDimensions[0],limits.maxViewportDimensions[1],limits.viewportBoundsRange[0],limits.viewportBoundsRange[1],{format.value.maxExtent.width,format.value.maxExtent.height}) || !(format.value.sampleCounts&vk::SampleCountFlagBits::e1)) {
                failed=true;Log("host-extent-format-or-device-limit-native-fallback");return false;
            }
            const VkImageCreateInfo raw=static_cast<VkImageCreateInfo>(ci);
            const VmaAllocationCreateInfo alloc{.flags=VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
            VmaAllocationInfo allocation_info{};
            if(vmaCreateImage(e->allocator,&raw,&alloc,&e->image,&e->allocation,&allocation_info)!=VK_SUCCESS) { failed=true;Log("allocation-native-fallback");return false; }
            e->allocation_bytes=allocation_info.size;
            ++diag.allocations;diag.allocation_bytes=allocation_info.size;
            if(allocation_info.size>format.value.maxResourceSize) {failed=true;Log("format-max-resource-size-native-fallback");return false;}
            vk::ImageViewCreateInfo vi{.image=vk::Image(e->image),.viewType=vk::ImageViewType::e2D,.format=vk::Format::eD32SfloatS8Uint,.subresourceRange={Aspects,0,1,0,1}};
            auto result=e->device.createImageViewUnique(vi);
            if(result.result!=vk::Result::eSuccess) { failed=true;Log("view-allocation-native-fallback");return false; }
            e->attachment=std::move(result.value);
            entry=std::move(e);Log("allocate-within-budget");
        }
        if(!entry->state.valid) Copy(false);
        return true;
    }
};
std::unordered_map<Runtime*,std::unique_ptr<State>> states;
OwnerGate owner_gate;
std::atomic<u32> closing_owner_tid{};
std::atomic<Runtime*> bound_runtime{};
std::function<bool()> recording_owner_check;
std::function<void()> final_owner_drain;
State* Get(Runtime& r) {
    if(bound_runtime.load(std::memory_order_acquire)!=&r) return nullptr;
    // No map access or GPU work is permitted on an unhanded-off guest thread.
    if(!recording_owner_check || !recording_owner_check()) std::abort();
    auto it=states.find(&r);return it==states.end()?nullptr:it->second.get();
}
}
unsigned Scale() {
    static const unsigned scale=[] {const char* p=std::getenv("BB_SUN_ATLAS_PROXY_SCALE");return p && std::strcmp(p,"1")==0?1u:p && std::strcmp(p,"2")==0?2u:0u;}();return scale;
}
bool Enabled() {return Scale()!=0;}
void Start(Runtime& r,VideoCore::TextureCache& t,std::function<bool()> owner_check,std::function<void()> final_drain) {
    if(!Enabled() || owner_gate.GetPhase()!=OwnerGate::Phase::Running || bound_runtime.load(std::memory_order_acquire)) return;
    recording_owner_check=std::move(owner_check);final_owner_drain=std::move(final_drain);
    states.emplace(&r,std::make_unique<State>(State{r,t}));
    bound_runtime.store(&r,std::memory_order_release);
}
void Stop(Runtime& r) {
    if(auto* s=Get(r)) {
        s->Drop();s->Summary("stop");r.GetScheduler().Finish();states.erase(&r);
        PublishSnapshot({},std::memory_order_release);
        bound_runtime.store(nullptr,std::memory_order_release);
    }
}
void UnmapOnOwner(std::function<AmdGpu::Liverpool*()> dispatcher,bool on_recording_thread,
                  std::function<bool()> owner_check,std::function<void()> work) {
    owner_gate.Run(owner_check,
        [on_recording_thread]{return on_recording_thread || static_cast<u32>(gettid())==closing_owner_tid.load(std::memory_order_acquire);},
        [dispatcher=std::move(dispatcher)](auto callback){
            // Neither pointer lookup happens before the gate lease. Shutdown
            // waits for the callback and semaphore completion before destroying it.
            auto* liverpool=dispatcher();if(!liverpool) std::abort();
            liverpool->SendCommand<true>(std::move(callback));
        },std::move(work));
}
void BeginOwnerShutdown(AmdGpu::Liverpool* liverpool) {
    if(!Enabled()) return;
    if(liverpool) closing_owner_tid.store(liverpool->GetGpuCommandProcessorThreadId(),std::memory_order_release);
    owner_gate.BeginClose(); // no TC, pending-operation, or runtime-memory lock held
    if(!liverpool) {
        if(bound_runtime.load(std::memory_order_acquire)) std::abort();
        return;
    }
    auto stop_all=[] {
        // ProcessCommands drained StageB before this callback; no cache mutex was acquired by the waiter.
        if(auto* r=bound_runtime.load(std::memory_order_acquire)) Stop(*r);
    };
    if(static_cast<u32>(gettid())==closing_owner_tid.load(std::memory_order_acquire)) stop_all();
    else liverpool->SendCommand<true>(std::move(stop_all));
}
void CompleteOwnerShutdown() {
    if(!Enabled()) return;
    // Called inside Liverpool's destructor after its process thread joined, while
    // its fields remain alive for existing BufferCache owner-ID checks. The final
    // B packets drain without A services, before Presenter resets command buffers.
    if(owner_gate.GetPhase()!=OwnerGate::Phase::Closing || bound_runtime.load(std::memory_order_acquire)) std::abort();
    if(final_owner_drain) final_owner_drain();
    final_owner_drain={};recording_owner_check={};
    owner_gate.CompleteClose();
}
void NextFrame(Runtime& r) { if(auto* s=Get(r)) { s->previous_frame=s->frame;++s->frame;s->PeriodicSummaryAt(std::chrono::steady_clock::now()); } }
void BeginDraw(Runtime& r,bool exact) { if(auto* s=Get(r)) {s->exact_pair=exact;s->active_scaled_clip_window=false;s->sync.BeginDraw();} }
bool ActiveScaledClipWindow(Runtime& r) {auto* s=Get(r);return s && s->active_scaled_clip_window;}
bool DynamicRouteChanged(Runtime& r) {
    auto* s=Get(r);return s && s->sync.DynamicRouteChanged(s->active_scaled_clip_window);
}
bool CanUseNativeMemo(Runtime& r,std::span<const VideoCore::ImageId> ids) {
    if(!Enabled()) return true;
    const auto f=std::atomic_load_explicit(&family_snapshot,std::memory_order_acquire);
    if(!f || f->runtime!=&r || bound_runtime.load(std::memory_order_acquire)!=&r) return false;
    // This is a recording-owner helper, never the non-owner leaf predicate above.
    auto* s=Get(r);if(!s || s->exact_pair) return false;
    auto* original=s->Original();
    const bool family_current=original && ValidIdentity(f->identity) && Identity(*original)==f->identity;
    if(!MemoContextReady(true,true,true,true,s->exact_pair,family_current)) return false;
    for(const auto id:ids) {
        if(!id) continue; // explicit null attachment/descriptor slot
        const auto* image=s->cache.TryGetImage(id,0);
        if(!image || ClassifyAlias(f->identity,Identity(*image))!=AliasClass::Ordinary) return false;
    }
    return true;
}

void CommitAttachment(Runtime& r,const RenderState& state) {
    auto* s=Get(r);if(!s || !s->sync.SelectedAttachment()) return;
    // Attachment selection and this owner-thread commit must refer to one live view.
    // Never issue a draw using a retired/replaced view if an alias changed meanwhile.
    if(!s->entry || state.depth_stencil_attachment.image_view!=*s->entry->attachment ||
       state.depth_stencil_attachment.image_layout!=vk::ImageLayout::eDepthStencilAttachmentOptimal)
        std::abort();
    auto& e=*s->entry;
    const bool valid=e.state.valid;
    auto* original=s->Original();
    if(!original || original->image_uid!=e.uid || original->GetImage()!=e.native || original->backing!=e.native_backing) std::abort();
    // A native write between selection and commit needs the normal reinitialization.
    if(!valid && !s->Ensure()) std::abort();
    const bool same_scope=r.GetScheduler().IsRenderingWith(state) &&
        !state.depth_stencil_attachment.depth_clear && !state.depth_stencil_attachment.stencil_clear;
    constexpr auto tests=vk::PipelineStageFlagBits2::eEarlyFragmentTests|vk::PipelineStageFlagBits2::eLateFragmentTests;
    constexpr auto writes=vk::AccessFlagBits2::eDepthStencilAttachmentRead|vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    const bool write_state=valid && e.layout==vk::ImageLayout::eDepthStencilAttachmentOptimal &&
        e.last_stage==tests && e.last_access==writes;
    if(s->sync.FinishWrite(write_state,same_scope)) {
        ++s->write_repairs;
        s->Transition(vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests|vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead|vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
    } else if(same_scope) ++s->write_coalesced;
    // Alias reads may have resolved the pre-draw conservative dirty mark.
    if(!e.state.Dirty()) e.state.Written();
    original->flags|=VideoCore::ImageFlagBits::GpuModified;
}

void Observe(Runtime& r,VideoCore::ImageId id) {
    auto* s=Get(r);if(!s || !s->exact_pair || s->uid || s->failed) return;
    auto& i=s->cache.GetImage(id);const auto& f=i.info;
    if(!RecognizedSize({f.size.width,f.size.height},Scale()) || f.size.depth!=1 || f.pixel_format!=vk::Format::eD32SfloatS8Uint || f.resources.levels!=1 || f.resources.layers!=1 || f.num_samples!=1 || !f.props.is_depth || !f.props.has_stencil || i.backing->num_samples!=1) return;
    s->candidate=id;s->uid=i.image_uid;s->seen=s->frame;
    s->diag={};s->diag.uid=s->uid;s->diag.address=f.guest_address;s->diag.seen_frame=s->frame;
    PublishSnapshot(std::make_shared<const FamilySnapshot>(FamilySnapshot{&r,Identity(i),i.image_uid,f.guest_address,f.stencil_addr,f.guest_size,f.stencil_size}),std::memory_order_release);
    s->Log("authorize-exact-sun-pair-high-family-next-frame");
}
std::optional<Target> Attachment(Runtime& r,VideoCore::ImageId id,bool clip_enabled) {
    auto* s=Get(r);if(!s || !s->uid || s->copying) return {};
    auto& i=s->cache.GetImage(id);if(i.image_uid!=s->uid) return {};
    const bool scaled_clip_window=QualifiedClipWindow(Scale(),clip_enabled);
    if(scaled_clip_window && !s->scaled_clip_window_supported.has_value()) {
        const auto limits=r.GetInstance().GetPhysicalDevice().getProperties().limits;
        s->scaled_clip_window_supported=ClipWindowSupported(limits.maxViewportDimensions[0],limits.maxViewportDimensions[1],limits.viewportBoundsRange[0],limits.viewportBoundsRange[1]);
    }
    if(scaled_clip_window && !*s->scaled_clip_window_supported) {
        s->diag.clip_disabled_fallback=true;
        // Permanent fallback must remain visible even after the event budget is exhausted.
        std::printf("BB_SUN_ATLAS_PROXY_UNSUPPORTED: frame=%llu uid=%llu scale=2 clip_disabled_fallback=1 reason=expanded-window-device-limits action=resolve-retire-permanent-native\n",(unsigned long long)s->frame,(unsigned long long)s->uid);std::fflush(stdout);
        if(s->entry) s->Drop();
        else {PublishSnapshot({},std::memory_order_release);s->candidate={};s->uid=0;}
        s->failed=true;return {};
    }
    if(!s->Ensure()) return {};
    auto& scheduler=r.GetScheduler();const auto& current=scheduler.GetRenderState();
    const bool write_state=s->entry->state.valid && s->entry->layout==vk::ImageLayout::eDepthStencilAttachmentOptimal;
    const bool own_scope=scheduler.IsRenderingWith(current) &&
        !current.depth_stencil_attachment.depth_clear && !current.depth_stencil_attachment.stencil_clear &&
        current.depth_stencil_attachment.image_view==*s->entry->attachment &&
        current.depth_stencil_attachment.image_layout==vk::ImageLayout::eDepthStencilAttachmentOptimal;
    if(s->sync.TentativeWrite(write_state,own_scope)) ++s->write_tentative;
    else s->Transition(vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests|vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead|vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
    ++s->diag.attachments;s->diag.last_attachment_frame=s->frame;
    s->entry->state.Written();i.flags|=VideoCore::ImageFlagBits::GpuModified;
    s->active_scaled_clip_window=scaled_clip_window;
    return Target{*s->entry->attachment,s->entry->layout,s->entry->width,s->entry->height};
}
std::optional<Target> Sample(Runtime& r,VideoCore::ImageId id,const VideoCore::ImageViewInfo& info) {
    auto* s=Get(r);if(!s || !s->uid || s->copying || !s->entry || !s->entry->state.valid) return {};
    auto& i=s->cache.GetImage(id);if(i.image_uid!=s->uid || i.GetImage()!=s->entry->native || i.backing!=s->entry->native_backing || !ProxyViewEligible(info.is_storage,info.type==AmdGpu::ImageType::Color2D,info.min_lod,info.range.base.level,info.range.base.layer,info.range.extent.levels,info.range.extent.layers,LiverpoolToVK::IsFormatDepthCompatible(info.format),LiverpoolToVK::IsFormatStencilCompatible(info.format))) return {};
    s->Transition(vk::ImageLayout::eDepthStencilReadOnlyOptimal,vk::PipelineStageFlagBits2::eAllGraphics|vk::PipelineStageFlagBits2::eComputeShader,vk::AccessFlagBits2::eShaderSampledRead);
    for(const auto& v:s->entry->views) if(v.first==info) { s->NoteSample();return Target{*v.second,s->entry->layout,s->entry->width,s->entry->height}; }
    auto result=s->entry->device.createImageViewUnique({.image=vk::Image(s->entry->image),.viewType=vk::ImageViewType::e2D,.format=vk::Format::eD32SfloatS8Uint,.components=info.mapping,.subresourceRange={vk::ImageAspectFlagBits::eDepth,0,1,0,1}});
    if(result.result!=vk::Result::eSuccess) { s->Log("sample-view-native-fallback");return {}; }
    auto view=std::move(result.value);
    auto handle=*view;s->entry->views.emplace_back(info,std::move(view));s->NoteSample();return Target{handle,s->entry->layout,s->entry->width,s->entry->height};
}
void NativeAccess(Runtime& r,VideoCore::Image& i,vk::AccessFlags2 access) {
    // Unrelated native images never inspect the mutable owner-only registry.
    if(!MayBelong(r,i)) return;
    auto* s=Get(r);if(!s || s->copying || !s->Matches(i)) return;
    if(s->entry->state.Dirty()) s->Copy(true);
    if(access&Writes) { ++s->diag.native_writes;if(s->entry->state.valid) { ++s->diag.invalidations;s->diag.last_invalidation_frame=s->frame; } s->entry->state.NativeWritten();s->Log("native-write-invalidates-proxy"); }
}
void Retire(Runtime& r,VideoCore::Image& i) {
    if(!MayBelong(r,i)) return;
    if(auto* s=Get(r);s && (s->Matches(i) || i.image_uid==s->uid)) {
        if(s->entry) s->Drop();
        else { s->Summary("retire-without-proxy");PublishSnapshot({},std::memory_order_release);s->candidate={};s->uid=0; }
    }
}
}

