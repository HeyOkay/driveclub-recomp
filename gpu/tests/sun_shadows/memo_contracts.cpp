#include "sun_atlas_proxy_alias_policy.h"
#include "sun_atlas_proxy_policy.h"
#include "sun_atlas_proxy_sync_policy.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
using namespace Vulkan::SunAtlasProxy;
void check(bool x, const char* s) {
    if (!x)
        throw std::runtime_error(s);
}
int main() {
    AliasIdentity family{1, 2, 3, {1000, 100}, {2000, 100}};
    AliasIdentity ordinary{10, 20, 30, {3000, 100}, {4000, 100}};
    check(ClassifyAlias(family, ordinary) == AliasClass::Ordinary,
          "disjoint complete image is ordinary");
    for (int axis = 0; axis < 3; ++axis) {
        auto a = ordinary;
        if (axis == 0)
            a.uid = family.uid;
        if (axis == 1)
            a.native = family.native;
        if (axis == 2)
            a.backing = family.backing;
        check(ClassifyAlias(family, a) == AliasClass::Alias,
              "UID/native/backing each independently rejects memo");
    }
    for (int f = 0; f < 2; ++f)
        for (int c = 0; c < 2; ++c) {
            auto a = ordinary;
            auto& r = c ? a.stencil : a.primary;
            r = {f ? 2050u : 1050u, 20};
            check(ClassifyAlias(family, a) == AliasClass::Alias,
                  "all four current/family primary/stencil overlaps rejected");
        }
    auto edge = ordinary;
    edge.primary = {1100, 100};
    edge.stencil = {2100, 100};
    check(ClassifyAlias(family, edge) == AliasClass::Ordinary,
          "half-open adjacent ranges do not overlap");
    auto noStencil = ordinary;
    noStencil.stencil = {};
    check(ClassifyAlias(family, noStencil) == AliasClass::Ordinary,
          "absent stencil is valid optional range");
    auto depthOnly = ordinary;
    depthOnly.stencil = ActiveStencil(false, {0, 65536});
    check(ClassifyAlias(family, depthOnly) == AliasClass::Ordinary,
          "ordinary depth-only constructor's inactive stencil bytes do not imply "
          "alias");
    depthOnly.stencil = ActiveStencil(false, {1050, 20});
    check(ClassifyAlias(family, depthOnly) == AliasClass::Ordinary,
          "inactive stencil register address has no Vulkan stencil access");
    depthOnly.stencil = ActiveStencil(true, {1050, 20});
    check(ClassifyAlias(family, depthOnly) == AliasClass::Alias,
          "active stencil address still matches family primary");
    for (int missing = 0; missing < 7; ++missing) {
        auto a = ordinary;
        if (missing == 0)
            a.uid = 0;
        if (missing == 1)
            a.native = 0;
        if (missing == 2)
            a.backing = 0;
        if (missing == 3)
            a.primary.address = 0;
        if (missing == 4)
            a.primary.bytes = 0;
        if (missing == 5)
            a.stencil = {0, 4};
        if (missing == 6)
            a.stencil = {5000, 0};
        check(ClassifyAlias(family, a) == AliasClass::Unknown,
              "missing/malformed identity refuses memo");
    }
    for (bool stencil : {false, true}) {
        auto a = ordinary;
        auto& range = stencil ? a.stencil : a.primary;
        range = {std::numeric_limits<std::uint64_t>::max() - 4, 8};
        check(ClassifyAlias(family, a) == AliasClass::Unknown,
              "overflow is unknown, never proven ordinary");
    }
    auto unknownHost = ordinary;
    unknownHost.primary = {};
    unknownHost.stencil = {};
    check(ClassifyAlias(family, unknownHost) == AliasClass::Unknown,
          "zero guest ranges are unknown for memo");
    check(ClassifyAlias(family, unknownHost) != AliasClass::Alias,
          "unknown ordinary host identity cannot trigger native coherence writes");
    auto incomplete = ordinary;
    incomplete.stencil = {0, 65536};
    check(ClassifyAlias(family, incomplete) == AliasClass::Unknown,
          "malformed unrelated stencil denies memo");
    check(ClassifyAlias(family, incomplete) != AliasClass::Alias,
          "incomplete ordinary metadata is not a coherence alias");
    incomplete.primary = {1050, 20};
    check(ClassifyAlias(family, incomplete) == AliasClass::Alias,
          "valid primary overlap wins despite malformed stencil");
    incomplete.native = 0;
    incomplete.backing = 0;
    incomplete.uid = 0;
    check(ClassifyAlias(family, incomplete) == AliasClass::Alias,
          "positive valid overlap not hidden by missing native identity");
    for (int f = 0; f < 2; ++f)
        for (int c = 0; c < 2; ++c) {
            auto a = ordinary;
            auto& r = c ? a.stencil : a.primary;
            r = {f ? 2050u : 1050u, 20};
            auto& other = c ? a.primary : a.stencil;
            other = {0, 65536};
            check(ClassifyAlias(family, a) == AliasClass::Alias,
                  "each independently valid overlap survives other malformed range");
        }
    unsigned spuriousNativeWrites = 0, deniedMemo = 0;
    for (int i = 0; i < 1000; ++i) {
        auto result = ClassifyAlias(family, unknownHost);
        if (result == AliasClass::Alias)
            ++spuriousNativeWrites;
        if (result != AliasClass::Ordinary)
            ++deniedMemo;
    }
    check(spuriousNativeWrites == 0 && deniedMemo == 1000,
          "unknown targets deny memo without repeated coherence invalidation");
    unsigned ready = 0;
    for (unsigned mask = 0; mask < 64; ++mask) {
        bool x = MemoContextReady(mask & 1, mask & 2, mask & 4, mask & 8, mask & 16, mask & 32);
        if (x)
            ++ready;
    }
    check(ready == 1, "snapshot/runtime/binding/owner/family gates all "
                      "mandatory; exact pair excluded");
    // Crucial initial-barrier interleavings: selection is active even without
    // tentative elision.
    SyncPolicy policy;
    Coherence co;
    co.Filled();
    co.Written();
    policy.BeginDraw();
    check(!policy.TentativeWrite(false, false), "initial writer emits early barrier");
    check(policy.SelectedAttachment() && !policy.PendingWrite(),
          "all selected writers reach final validation");
    co.Resolved();
    check(!co.Dirty(), "alias read resolves before first pending draw");
    check(policy.FinishWrite(false, false),
          "transfer-source layout after alias resolve restores DS at final hook");
    if (!co.Dirty())
        co.Written();
    check(co.Dirty(), "final pending draw restores dirty state after alias read");
    policy.BeginDraw();
    policy.TentativeWrite(false, false);
    co.Resolved();
    co.NativeWritten();
    check(!co.valid, "native write invalidates initial selected attachment");
    co.Filled();
    check(policy.FinishWrite(false, false),
          "invalidated/refilled attachment must transition even without "
          "tentative elision");
    if (!co.Dirty())
        co.Written();
    check(co.valid && co.Dirty(), "refilled final write remains coherent");
    policy.BeginDraw();
    policy.TentativeWrite(false, false);
    check(!policy.FinishWrite(true, false),
          "early barrier plus unrelated flush needs no duplicate WAW");
    check(!policy.SelectedAttachment(), "commit consumes selected flag");
    std::puts("PASS production alias/context policy: native/backing/UID, all4 "
              "depth/stencil overlaps, malformed/overflow, exact-family readiness, and "
              "actualCoherence initial-barrier alias/native interleavings");
}
