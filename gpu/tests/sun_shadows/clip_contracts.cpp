#include "sun_atlas_proxy_2x_policy.h"
#include "sun_atlas_proxy_route.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <numeric>
#include <vector>
using namespace Vulkan::SunAtlasProxy;
struct Fraction {
    long long n, d;
    Fraction(long long n_, long long d_ = 1) : n(n_), d(d_) {
        assert(d);
        if (d < 0) {
            n = -n;
            d = -d;
        }
        const auto g = std::gcd(n, d);
        n /= g;
        d /= g;
    }
    bool operator==(const Fraction&) const = default;
    friend Fraction operator+(Fraction a, Fraction b) {
        return {a.n * b.d + b.n * a.d, a.d * b.d};
    }
    friend Fraction operator-(Fraction a, Fraction b) {
        return {a.n * b.d - b.n * a.d, a.d * b.d};
    }
    friend Fraction operator*(Fraction a, Fraction b) {
        return {a.n * b.n, a.d * b.d};
    }
    friend Fraction operator/(Fraction a, Fraction b) {
        return {a.n * b.d, a.d * b.n};
    }
};
int main() {
    assert((ExpandedClipWindow(32768, 32768) == Size{32768, 32768}));
    assert(ClipWindowSupported(32768, 32768, -65536, 65536)); // actual NVIDIA
    assert(!ClipWindowSupported(32768, 32768, -32768,
                                32767)); // old conservative fixture
    assert(!ClipWindowSupported(16384, 16384, -65536, 65536));
    assert(!ClipWindowSupported(32768, 32768, 1, 65536));
    assert(!ClipWindowSupported(32768, 32768, -65536, std::numeric_limits<float>::infinity()));
    assert(!ClipWindowSupported(0, 32768, -65536, 65536));
    assert(QualifiedClipWindow(2, false));
    assert(!QualifiedClipWindow(2, true));
    assert(!QualifiedClipWindow(1, false));
    assert(!QualifiedClipWindow(0, false));
    struct Push {
        float xscale, xoffset, yscale, yoffset;
    };
    std::array<float, 2> twice{2, 2};
    Push native{3, -5, -7, 11}, keep = native, multiply = native;
    ScaleTargetPush(keep, twice, true);
    ScaleTargetPush(multiply, twice, false);
    assert(keep.xscale == 3 && keep.xoffset == -5 && keep.yscale == -7 && keep.yoffset == 11);
    assert(multiply.xscale == 6 && multiply.xoffset == -10 && multiply.yscale == -14 &&
           multiply.yoffset == 22);
    assert(LogicalFragCoordEncoding({9472, 10752}, {18944, 21504}) == (3840u | (2160u << 16)));
    assert(LogicalFragCoordEncoding({9472, 10752}, {9472, 10752}) == 0);
    assert(LogicalFragCoordEncoding({9216, 8192}, {9216, 8192}) == 0);
    assert(LogicalFragCoordEncoding({9472, 10752}, {18944, 10752}) == 0);
    const auto packed = LogicalFragCoordEncoding({9472, 10752}, {18944, 21504});
    assert(Fraction(1920, packed & 65535u) == Fraction(1, 2) &&
           Fraction(1080, packed >> 16u) == Fraction(1, 2));
    // Exact arithmetic, using the production push/window policies rather than a
    // w==1 approximation. Preserve native NDC and double only viewport extent.
    unsigned rational_cases = 0;
    for (long long w : {1, 2, 3, 4, 5, -1, -2, -3, -4, -5})
        for (long long x : {-20000, -5, 0, 5, 20000})
            for (long long xs : {1, 2, 3, 4})
                for (long long off : {-100, 0, 100}) {
                    Push p{float(xs), float(off), 1, 0};
                    ScaleTargetPush(p, twice, true);
                    const auto ndc = Fraction(x * xs + off, 8192) - Fraction(1);
                    const auto host_ndc = Fraction(x * static_cast<long long>(p.xscale) +
                                                       static_cast<long long>(p.xoffset),
                                                   8192) -
                                          Fraction(1);
                    const auto raster = (ndc / Fraction(w) + Fraction(1)) * Fraction(16384, 2);
                    const auto host = (host_ndc / Fraction(w) + Fraction(1)) *
                                      Fraction(ExpandedClipWindow(32768, 32768).width, 2);
                    assert(ndc == host_ndc && host == raster * Fraction(2));
                    ++rational_cases;
                }
    assert(rational_cases == 600);
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
    std::puts("PASS: expanded clip-window actual/unsupported limits, qualified "
              "flag policy, native/scaled push branches, logical FragCoord "
              "ratio,600 exact rational arbitrary-W mappings; existing2x "
              "size/copy/classifier/view/native/stencil guards");
}
