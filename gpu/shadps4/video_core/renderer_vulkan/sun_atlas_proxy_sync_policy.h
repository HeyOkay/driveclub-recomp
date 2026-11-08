#pragma once
namespace Vulkan::SunAtlasProxy {
// Owner-thread private policy; not part of Runtime/Rasterizer/Scheduler layout.
class SyncPolicy {
    bool pending_write = false, selected_attachment = false;
    bool memo_seen = false, memo_clip = false;

  public:
    void BeginDraw() {
        pending_write = false;
        selected_attachment = false;
    }
    bool TentativeWrite(bool write_state, bool own_open_scope) {
        selected_attachment = true;
        pending_write = write_state && own_open_scope;
        return pending_write;
    }
    bool PendingWrite() const {
        return pending_write;
    }
    bool SelectedAttachment() const {
        return selected_attachment;
    }
    bool FinishWrite(bool write_state, bool final_same_open_scope) {
        const bool repair =
            selected_attachment && (!write_state || (pending_write && !final_same_open_scope));
        pending_write = false;
        selected_attachment = false;
        return repair;
    }
    static bool MergeRead(bool sampled_read_request, bool same_layout, bool same_access,
                          bool stages_covered) {
        return sampled_read_request && same_layout && same_access && stages_covered;
    }
    bool DynamicRouteChanged(bool clip) {
        const bool changed = !memo_seen || memo_clip != clip;
        memo_seen = true;
        memo_clip = clip;
        return changed;
    }
};
} // namespace Vulkan::SunAtlasProxy
