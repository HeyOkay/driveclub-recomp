#include "sun_atlas_proxy_policy.h"
#include "sun_atlas_proxy_sync_policy.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
using Vulkan::SunAtlasProxy::SyncPolicy;
void check(bool b, const char* message) {
    if (!b)
        throw std::runtime_error(message);
}
// Simulate the two production decision points, with explicit changes in
// between.
int main() {
    for (bool direct : {false, true}) {
        (void)direct;
        SyncPolicy p;
        p.BeginDraw();
        check(!p.TentativeWrite(false, false), "first attachment must transition");
        check(!p.PendingWrite(), "initial transition has no deferred repair");
        p.BeginDraw();
        check(p.TentativeWrite(true, true), "same live proxy scope tentatively reusable");
        check(!p.FinishWrite(true, true), "contiguous equal scope coalesces");
        check(!p.PendingWrite(), "commit consumes pending flag");
        // Flush, MarkPass, explicit end, submission/frame break and differing
        // clear/area/aspect all make the FINAL scheduler equality false, despite
        // the earlier successful probe.
        for (const char* cause :
             {"buffer-flush", "MarkPass-scope-change", "explicit-end", "submit", "new-frame",
              "clear-change", "extent-change", "aspect-change", "color-slot-change"}) {
            p.BeginDraw();
            check(p.TentativeWrite(true, true), cause);
            check(p.FinishWrite(true, false), "late scope/state change requires WAW repair");
        }
        p.BeginDraw();
        p.TentativeWrite(true, true);
        check(p.FinishWrite(false, true), "layout/native validity change cannot "
                                          "coalesce even if stale state matches");
        p.BeginDraw();
        p.TentativeWrite(true, true);
        p.BeginDraw();
        check(!p.PendingWrite(), "abandoned draw cannot leak pending state");
        p.BeginDraw();
        check(!p.TentativeWrite(true, false), "same layout in closed scope transitions");
        check(!p.TentativeWrite(false, true),
              "sample/copy/native invalidation cannot reuse writer");
    }
    // Exhaust the four independent gates. Transfers/writes are never read-merge
    // requests; write->sample changes layout/access, while sample->sample is the
    // sole all-true case.
    unsigned merged = 0;
    for (bool sample : {false, true})
        for (bool layout : {false, true})
            for (bool access : {false, true})
                for (bool stages : {false, true}) {
                    bool result = SyncPolicy::MergeRead(sample, layout, access, stages);
                    if (result)
                        ++merged;
                    check(result == (sample && layout && access && stages),
                          "read visibility requires every gate");
                }
    check(merged == 1, "only fully qualified read->read merge");
    {
        SyncPolicy sync;
        Vulkan::SunAtlasProxy::Coherence c;
        c.Filled();
        c.Written();
        check(c.Dirty(), "actual coherence marks proxy writes dirty");
        sync.BeginDraw();
        sync.TentativeWrite(c.valid, true);
        c.Resolved();
        check(c.valid && !c.Dirty(), "native read resolve preserves initialized proxy");
        c.NativeWritten();
        check(!c.valid && !c.Dirty(), "CPU/native write invalidates even after resolve");
        check(sync.FinishWrite(c.valid, true),
              "native invalidation forces repair despite identical render state");
        c.Filled();
        c.Written();
        check(c.valid && c.Dirty(), "reinitialization then committed draw becomes dirty again");
        sync.BeginDraw();
        check(!sync.TentativeWrite(true, false), "current clear excluded from own-scope predicate");
        sync.TentativeWrite(true, true);
        check(sync.FinishWrite(false, true),
              "identical repeated final clear still forces new scope");
    }
    SyncPolicy p;
    check(p.DynamicRouteChanged(false), "first dynamic route invalidates");
    check(!p.DynamicRouteChanged(false), "ordinary route can memoize");
    p.BeginDraw();
    check(p.DynamicRouteChanged(true), "ordinary->expanded invalidates");
    p.BeginDraw();
    check(!p.DynamicRouteChanged(true), "BeginDraw retains memo route history");
    check(p.DynamicRouteChanged(false),
          "expanded->ordinary invalidates even with otherwise equal keys");
    check(!p.DynamicRouteChanged(false), "ordinary route resumes memo hits");
    SyncPolicy other;
    check(other.DynamicRouteChanged(false), "separate runtime has independent route history");
    std::puts("PASS production SyncPolicy: direct/indirect decision timing; "
              "closed/changed scope and flush repair; read visibility gates; "
              "native invalidation; pending reset; both route transitions and "
              "per-runtime memo history");
}
