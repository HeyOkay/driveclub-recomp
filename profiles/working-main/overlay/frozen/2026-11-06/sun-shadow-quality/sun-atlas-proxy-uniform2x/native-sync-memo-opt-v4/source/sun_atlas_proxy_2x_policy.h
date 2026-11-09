#pragma once
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cmath>
namespace Vulkan::SunAtlasProxy {
struct Size {std::uint32_t width{},height{};bool operator==(const Size&)const=default;};
// Preserve the shader's original hardware-window NDC conversion, then scale
// raster coordinates by enlarging that same window. The native window is
// bounded at16K even when the device supports a larger viewport.
inline Size ExpandedClipWindow(unsigned max_width,unsigned max_height) {
    return {std::min(max_width,16384u)*2u,std::min(max_height,16384u)*2u};
}
inline bool ClipWindowSupported(unsigned max_width,unsigned max_height,float bounds_min,float bounds_max) {
    const auto w=ExpandedClipWindow(max_width,max_height);
    return w.width && w.height && w.width<=max_width && w.height<=max_height &&
        std::isfinite(bounds_min) && std::isfinite(bounds_max) && bounds_min<=0 &&
        float(w.width)<=bounds_max && float(w.height)<=bounds_max;
}
inline bool QualifiedClipWindow(unsigned scale,bool clip_enabled) {return scale==2 && !clip_enabled;}
template<class Push,class Scale> inline void ScaleTargetPush(Push& push,const Scale& scale,bool clip_window) {
    if(clip_window) return; // keep native NDC for arbitrary exported clip W
    push.xscale*=scale[0];push.xoffset*=scale[0];
    push.yscale*=scale[1];push.yoffset*=scale[1];
}
inline std::uint32_t LogicalFragCoordEncoding(Size guest,Size host) {
    // Existing FragCoord consumer decodes1920/width and1080/height. This is a
    // scale ratio, not a claim that the Sun atlas has scene dimensions.
    return guest.width && guest.height && host.width==guest.width*2u && host.height==guest.height*2u
        ? 3840u | (2160u<<16u) : 0u;
}
inline bool RecognizedSize(Size n,unsigned scale) {
    return (scale==1 && ((n==Size{9216,8192}) || (n==Size{9472,10752}))) || (scale==2 && n==Size{9472,10752});
}
inline Size HostSize(Size n,unsigned scale) {
    if(!RecognizedSize(n,scale)) return {};
    return {n.width*scale,n.height*scale};
}
template<class State,class Scale> inline void OverrideTarget(State& state,Scale& scale,Size guest,Size host) {
    state.width=host.width;state.height=host.height;
    scale={float(host.width)/float(guest.width),float(host.height)/float(guest.height)};
}
struct CopySizes {Size src,dst;};
inline CopySizes CopyGeometry(Size guest,Size host,bool to_native) {return to_native?CopySizes{host,guest}:CopySizes{guest,host};}
inline bool SemanticProxyEligible(bool enabled,unsigned bindings,bool native,bool written,bool no_mip_fallback) {
    return enabled && bindings==1 && !native && !written && no_mip_fallback;
}
template<class Vector> inline void AppendEligibility(Vector& v,bool eligible) {v.push_back(eligible);}
inline bool ProxyViewEligible(bool storage,bool is_2d,unsigned min_lod,unsigned level,unsigned layer,unsigned levels,unsigned layers,bool depth,bool stencil) {
    return !storage && is_2d && !min_lod && !level && !layer && levels==1 && layers==1 && depth && !stencil;
}
inline bool DimensionsSupported(Size h,unsigned image_limit,unsigned fb_width,unsigned fb_height,unsigned vp_width,unsigned vp_height,float vp_min,float vp_max,Size format_max) {
    return h.width && h.height && h.width<=image_limit && h.height<=image_limit && h.width<=fb_width && h.height<=fb_height && h.width<=vp_width && h.height<=vp_height && vp_min<=0 && float(h.width)<=vp_max && float(h.height)<=vp_max && h.width<=format_max.width && h.height<=format_max.height;
}
}
