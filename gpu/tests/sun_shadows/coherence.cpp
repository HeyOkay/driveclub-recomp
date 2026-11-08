#include "sun_atlas_proxy_policy.h"
#include <cassert>
#include <cstdio>
using Vulkan::SunAtlasProxy::Coherence;
int main() {
    Coherence s;
    assert(!s.valid && !s.Dirty());
    s.Filled();
    assert(s.valid && !s.Dirty());
    s.Written();
    assert(s.Dirty());
    auto first = s.proxy_epoch;
    s.Resolved();
    assert(s.valid && !s.Dirty() && s.native_epoch == first);
    s.Written();
    assert(s.Dirty() && s.proxy_epoch != first);
    s.Resolved();
    s.NativeWritten();
    assert(!s.valid && !s.Dirty());
    // A native partial update followed by host fill must not resurrect an old
    // proxy epoch.
    auto native = s.native_epoch;
    s.Filled();
    assert(s.valid && !s.Dirty() && s.proxy_epoch == native);
    // Arbitrary alternating sequences characterize read-before-native-write
    // ordering.
    for (unsigned n = 0; n < 100000; ++n) {
        s.Written();
        s.Written();
        assert(s.Dirty());
        s.Resolved();
        assert(s.valid && s.proxy_epoch == s.native_epoch);
        s.NativeWritten();
        assert(!s.valid);
        s.NativeWritten();
        s.Filled();
        assert(s.valid && !s.Dirty());
    }
    std::puts("PASS: initialize, repeated proxy writes, native read resolve "
              "retaining proxy validity, native write invalidation, "
              "partial-update refill, 100000 alternating epochs");
}
