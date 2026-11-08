#pragma once
#include <cstdint>
namespace Vulkan::SunAtlasProxy {
// Production state machine: a resolve never discards the initialized host
// image; a native write always invalidates it, even if no resolve was
// necessary.
struct Coherence {
    std::uint64_t proxy_epoch = 0, native_epoch = 0;
    bool valid = false;
    bool Dirty() const {
        return valid && proxy_epoch != native_epoch;
    }
    void Filled() {
        valid = true;
        proxy_epoch = native_epoch;
    }
    void Written() {
        valid = true;
        ++proxy_epoch;
    }
    void Resolved() {
        native_epoch = proxy_epoch;
    }
    void NativeWritten() {
        valid = false;
        ++native_epoch;
        proxy_epoch = native_epoch;
    }
};
} // namespace Vulkan::SunAtlasProxy
