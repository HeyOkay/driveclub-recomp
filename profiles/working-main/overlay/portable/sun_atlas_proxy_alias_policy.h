#pragma once
#include <cstdint>
#include <limits>
namespace Vulkan::SunAtlasProxy {
struct AliasRange {std::uint64_t address{},bytes{};bool operator==(const AliasRange&) const=default;};
struct AliasIdentity {
    std::uint64_t uid{};std::uintptr_t native{},backing{};
    AliasRange primary{},stencil{};
    bool operator==(const AliasIdentity&) const = default;
};
enum class AliasClass {Ordinary,Alias,Unknown};
// Depth ImageInfo fills stencil metadata even for formats without a stencil aspect.
inline AliasRange ActiveStencil(bool has_stencil,AliasRange raw) {return has_stencil ? raw : AliasRange{};}
inline bool ValidRange(AliasRange r,bool optional=false) {
    if(optional && !r.address && !r.bytes) return true;
    return r.address && r.bytes && r.bytes<=std::numeric_limits<std::uint64_t>::max()-r.address;
}
inline bool ValidIdentity(const AliasIdentity& i) {
    return i.uid && i.native && i.backing && ValidRange(i.primary) && ValidRange(i.stencil,true);
}
inline bool Overlap(AliasRange a,AliasRange b) {
    return a.bytes && b.bytes && a.address<b.address+b.bytes && b.address<a.address+a.bytes;
}
inline AliasClass ClassifyAlias(const AliasIdentity& family,const AliasIdentity& current) {
    if((family.uid && family.uid==current.uid) || (family.native && family.native==current.native) ||
       (family.backing && family.backing==current.backing)) return AliasClass::Alias;
    // An independently valid positive overlap remains evidence even if another
    // interval or identity field is missing. Never add unchecked range endpoints.
    const AliasRange family_ranges[]{family.primary,family.stencil};
    const AliasRange current_ranges[]{current.primary,current.stencil};
    for(const auto a:family_ranges) for(const auto b:current_ranges)
        if(ValidRange(a) && ValidRange(b) && Overlap(a,b)) return AliasClass::Alias;
    // Unknown refuses memo reuse but is NOT affirmative coherence evidence.
    if(!ValidIdentity(family) || !ValidIdentity(current)) return AliasClass::Unknown;
    return AliasClass::Ordinary;
}
inline bool MemoContextReady(bool snapshot,bool runtime_match,bool bound,bool owner_state,bool exact_pair,
                             bool family_current) {
    return snapshot && runtime_match && bound && owner_state && !exact_pair && family_current;
}
}
