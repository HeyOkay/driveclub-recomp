#include "native_depth_grid_policy.h"
#include "projected-fixtures.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace Vulkan::NativeDepthGrid;
int main() {
    for (const auto& fixture : projected_fixtures) {
        assert(fixture.count <= 6);
        assert(ProjectedDepthProducer(true, false, true, true, false, fixture.attributes,
                                      fixture.depth_export));
    }
    assert(ProjectedDepthProducer(true, false, true, true, false, 3, false));
    assert(ProjectedDepthProducer(true, false, true, true, false, 4, false));
    assert(!ProjectedDepthProducer(false, false, true, true, false, 4, false));
    assert(!ProjectedDepthProducer(true, true, true, true, false, 4, false));
    assert(!ProjectedDepthProducer(true, false, false, true, false, 4, false));
    assert(!ProjectedDepthProducer(true, false, true, false, false, 4, false));
    assert(!ProjectedDepthProducer(true, false, true, true, true, 4, false));
    assert(!ProjectedDepthProducer(true, false, true, true, false, 0, false));
    assert(!ProjectedDepthProducer(true, false, true, true, false, 4, true));
    // Depth attachment write marker is NOT a shader-storage exclusion.
    assert(FullDepthAttachmentView(true, true, 0, 0, 1, 1));
    assert(FullDepthAttachmentView(true, false, 0, 0, 1, 1));
    assert(!FullDepthAttachmentView(false, true, 0, 0, 1, 1));
    assert(!FullDepthAttachmentView(true, true, 1, 0, 1, 1));
    assert(!FullDepthAttachmentView(true, true, 0, 1, 1, 1));
    assert(!FullDepthAttachmentView(true, true, 0, 0, 2, 1));
    assert(!FullDepthAttachmentView(true, true, 0, 0, 1, 2));
    assert(ProducerDepthDomain(0.f, 1.f, false));
    assert(!ProducerDepthDomain(0.f, 1.f, true));
    assert(!ProducerDepthDomain(-1.f, 1.f, false));
    assert(!ProducerDepthDomain(0.f, 2.f, false));
    assert(!ProducerDepthDomain(std::numeric_limits<float>::quiet_NaN(), 1.f, false));
    FarZeroGuards far{true, 0x83a2f6fd, 0x8612c289, true, false, true,
                      true, true,       true,       true, true,  true};
    assert(FarZeroNoChange(far));
    assert(FarZeroFacts(far) == 0xfff);
    far.pixel_shader = 0x34674cc2;
    assert(FarZeroNoChange(far));
    auto altered = far;
    altered.vertex_shader = 0;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.pixel_shader = 0;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.scope = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.only_vertex_fragment = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.fragment_depth_export = true;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.greater_equal = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.bias_disabled = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.viewport_zero_one = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.clip_enabled = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.clip_zero_to_w = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.depth_test = false;
    assert(!FarZeroNoChange(altered));
    altered = far;
    altered.depth_write = false;
    assert(!FarZeroNoChange(altered));
    // Dimension/mode qualification crutches are absent; actual render-pixel
    // jitter remains authoritative.
    const auto native4k = EffectiveJ(.3125f, 2160.f + .2037037f, 3840.f, -2160.f, 3840, 2160);
    assert(native4k && (*native4k)[1] == .20361328125f);
    const auto fsrRender = EffectiveJ(.1875f, 1080.f + .027777778f, 1920.f, -1080.f, 1920, 1080);
    assert(fsrRender && (*fsrRender)[1] == .02783203125f);
    assert(!EffectiveJ(.1875f, 1080.f + .027777778f, 3840.f, -2160.f, 1920,
                       1080)); // output dimensions cannot stand in for input grid
    Tracking generic;
    generic.runtime = 123;
    generic.session = 2;
    const Key native4kKey{10, 20, 30, 3840, 2160}, fsrKey{11, 21, 31, 1920, 1080};
    generic.Clear(native4kKey);
    generic.Draw(native4kKey, true, true, true, false, *native4k);
    assert(generic.Query(123, native4kKey, true));
    assert(!generic.Query(123, native4kKey,
                          false)); // feature-off context must fall back
    generic.Draw(native4kKey, true, false, true, false, {0, 0});
    assert(!generic.Query(123, native4kKey, true) &&
           generic.blocked); // actual AA-off unjittered writer poisons prior J
    generic.Clear(native4kKey);
    generic.Draw(native4kKey, true, true, true, false, *native4k);
    assert(!generic.Query(124, native4kKey, true));
    assert(!generic.Query(123, fsrKey,
                          true)); // resize/new physical source cannot reuse previous tag
    generic.NextFrame();
    generic.Clear(fsrKey);
    generic.Draw(fsrKey, true, true, true, false, *fsrRender);
    assert(generic.Query(123, fsrKey, true));
    auto changed = *fsrRender;
    changed[0] += .01f;
    generic.Draw(fsrKey, true, true, true, false, changed);
    assert(!generic.Query(123, fsrKey, true) &&
           generic.blocked); // modes/scale change cannot merge different grids
    assert(!ProjectedDepthProducer(false, false, true, true, false, 4,
                                   false)); // DriveAA inactive or legacy upscaler context
    const Key a{1, 2, 3, 2560, 1440}, b{4, 5, 6, 2560, 1440};
    const std::array<float, 2> j{.25f, -.2f};
    Tracking t;
    t.runtime = 123;
    t.session = 1;
    auto query = [&] { return t.Query(123, a, true).has_value(); };
    t.Draw(a, true, true, true, false, j);
    assert(!query()); // retained unknown pixels are never qualified
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    assert(query());
    const auto valid = t;
    t.Draw(a, true, false, true, false, {0, 0}, true);
    assert(t == valid && query());
    auto wrong = a;
    ++wrong.uid;
    t.Draw(wrong, true, false, true, false, {0, 0}, true);
    assert(!t.Query(123, wrong, true));
    t = valid;
    t.Write(a, false);
    t.Draw(a, true, false, true, false, {0, 0}, true);
    assert(!query() && t.blocked && !t.WasCleared(a));
    t.Clear(a);
    t.Draw(a, true, false, true, false, {0, 0}, true);
    assert(!query() && t.blocked); // no tag creation from clear-only sky
    t = valid;
    t.Draw(a, true, false, true, false, {0, 0}, false);
    assert(!query() && t.blocked); // failed semantic guard poisons normally
    t = valid;
    const auto same = t;
    t.Draw(a, true, true, true, false, j);
    assert(t == same); // no hot-path publication
    t.Write(a, true);
    assert(t == same); // anticipated attachment barrier is not a write
    assert(!t.Query(123, a, false));
    assert(!t.Query(124, a, true));
    for (int field = 0; field < 5; ++field) {
        auto k = a;
        if (field == 0)
            ++k.uid;
        if (field == 1)
            ++k.native;
        if (field == 2)
            ++k.backing;
        if (field == 3)
            ++k.width;
        if (field == 4)
            ++k.height;
        assert(!t.Query(123, k, true));
    }
    // SwapDepthForm changes logical image container, never physical identity:
    // moved key works.
    auto moved = a;
    assert(t.Query(123, moved, true));
    t.Draw(a, true, true, true, false, {.1f, -.2f});
    assert(!query() && t.blocked && !t.WasCleared(a));
    t.Draw(b, true, true, true, false, j);
    t.Draw(a, true, true, true, false, j);
    assert(!query());
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    assert(query());
    t.Draw(a, true, false, true, false, j);
    assert(!query() && !t.WasCleared(a));
    t.Draw(b, true, true, true, false, j);
    t.Draw(a, true, true, true, false, j);
    assert(!query());
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    auto alias = a;
    alias.uid = 10;
    alias.width = 99;
    t.Write(alias, false);
    assert(!query());
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    t.Draw(alias, false, false, true, false, j);
    assert(!query() && !t.WasCleared(a));
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    t.Write(a, false);
    t.Draw(a, true, true, true, false, j);
    assert(!query()); // reverse copy poison prevents double J
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    t.Clear(alias);
    t.Draw(alias, true, true, true, false, j);
    assert(!t.WasCleared(a));
    t.Draw(a, true, true, true, false, {.1f, .2f});
    assert(!query());
    t.Clear(a);
    t.Draw(a, true, true, true, false, j);
    t.NextFrame();
    t.Draw(a, true, true, true, false, j);
    assert(!query());
    for (auto bad : {std::array<float, 2>{0, 0}, std::array<float, 2>{.6f, 0},
                     std::array<float, 2>{std::numeric_limits<float>::quiet_NaN(), 0}}) {
        t.Clear(a);
        t.Draw(a, true, true, true, false, bad);
        assert(!query());
    }
    const float requested = .2037037f;
    const float viewport_y = 1440.f + requested;
    auto actual = EffectiveJ(.25f, viewport_y, 2560.f, -1440.f, 2560, 1440);
    assert(actual && (*actual)[1] == viewport_y - 1440.f && (*actual)[1] != requested);
    assert(!EffectiveJ(0, 1440, 1280, -1440, 2560, 1440));
    assert(!EffectiveJ(0, 0, 2560, 1440, 2560, 1440));
    std::cout << "native grid policy contracts passed\n";
}
