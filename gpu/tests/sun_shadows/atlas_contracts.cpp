#include "sun_atlas_proxy_2x_policy.h"
#include "sun_atlas_proxy_route.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace Vulkan::SunAtlasProxy;
int main() {
    assert(RecognizedSize({9216, 8192}, 1));
    assert(!RecognizedSize({9216, 8192}, 2));
    assert(RecognizedSize({9472, 10752}, 1) && RecognizedSize({9472, 10752}, 2));
    assert(!RecognizedSize({9472, 10752}, 3));
    assert(!RecognizedSize({123, 456}, 2));
    assert((HostSize({9472, 10752}, 2) == Size{18944, 21504}));
    assert((HostSize({9216, 8192}, 2) == Size{}));
    struct State {
        unsigned width, height;
    };
    State state{17, 23};
    std::array<float, 2> scale{0.5f, 1.5f};
    OverrideTarget(state, scale, {9472, 10752}, {18944, 21504});
    assert(state.width == 18944 && state.height == 21504 && scale[0] == 2 && scale[1] == 2);
    OverrideTarget(state, scale, {9472, 10752}, {18944, 21504});
    assert(scale[0] == 2 && scale[1] == 2);
    auto fill = CopyGeometry({9472, 10752}, {18944, 21504}, false);
    auto resolve = CopyGeometry({9472, 10752}, {18944, 21504}, true);
    assert((fill.src == Size{9472, 10752}) && (fill.dst == Size{18944, 21504}));
    assert(resolve.src == fill.dst && resolve.dst == fill.src);
    auto one = CopyGeometry({9216, 8192}, {9216, 8192}, true);
    assert(one.src == one.dst);
    assert(DimensionsSupported({18944, 21504}, 32768, 32768, 32768, 32768, 32768, -32768, 32767,
                               {32768, 32768}));
    assert(!DimensionsSupported({18944, 21504}, 16384, 32768, 32768, 32768, 32768, -32768, 32767,
                                {32768, 32768}));
    assert(!DimensionsSupported({18944, 21504}, 32768, 16384, 32768, 32768, 32768, -32768, 32767,
                                {32768, 32768}));
    assert(!DimensionsSupported({18944, 21504}, 32768, 32768, 32768, 32768, 16384, -32768, 32767,
                                {32768, 32768}));
    assert(!DimensionsSupported({18944, 21504}, 32768, 32768, 32768, 32768, 32768, -32768, 32767,
                                {16384, 16384}));
    std::vector<bool> flat;
    AppendEligibility(flat, SemanticProxyEligible(true, 1, true, false, true)); // native camera
    AppendEligibility(flat, false);                                             // invalid sharp
    for (int i = 0; i < 3; ++i)
        AppendEligibility(flat, SemanticProxyEligible(true, 3, false, false,
                                                      false)); // expanded mip array
    AppendEligibility(flat, SemanticProxyEligible(true, 1, false, false, true)); // atlas
    assert(flat.size() == 6 && !flat[0] && !flat[1] && !flat[2] && !flat[3] && !flat[4] && flat[5]);
    assert(!SemanticProxyEligible(true, 1, true, false, true));
    assert(!SemanticProxyEligible(true, 1, false, true, true));
    assert(!SemanticProxyEligible(true, 1, false, false, false));
    assert(!SemanticProxyEligible(true, 2, false, false, true));
    assert(!SemanticProxyEligible(false, 1, false, false, true));
    assert(ProxyViewEligible(false, true, 0, 0, 0, 1, 1, true, false));
    assert(!ProxyViewEligible(true, true, 0, 0, 0, 1, 1, true, false));
    assert(!ProxyViewEligible(false, false, 0, 0, 0, 1, 1, true, false));
    assert(!ProxyViewEligible(false, true, 1, 0, 0, 1, 1, true, false));
    assert(!ProxyViewEligible(false, true, 0, 1, 0, 1, 1, true, false));
    assert(!ProxyViewEligible(false, true, 0, 0, 1, 1, 1, true, false));
    assert(!ProxyViewEligible(false, true, 0, 0, 0, 2, 1, true, false));
    assert(!ProxyViewEligible(false, true, 0, 0, 0, 1, 2, true, false));
    assert(!ProxyViewEligible(false, true, 0, 0, 0, 1, 1, false, false));
    assert(!ProxyViewEligible(false, true, 0, 0, 0, 1, 1, true, true));
    std::puts("PASS: exact old/new scale whitelist, host geometry, directional "
              "D/S copy geometry, device/format bounds, actual semantic "
              "classifier with flattened invalid/camera/mip/atlas mapping, "
              "view/native/stencil/write guards");
}
