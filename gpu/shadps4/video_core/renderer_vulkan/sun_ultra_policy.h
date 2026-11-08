#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <span>
#include <utility>
#ifdef BB_SUN_NATIVE_PAYLOADS
#include <sun_native_payloads.h>
#endif
namespace BbSunUltra {
enum class Kind { Unknown, Vertex, Fragment };
using ModuleKey=std::pair<std::uintptr_t,std::uintptr_t>;
inline bool RangeRequested(const char* value) { return value && std::strcmp(value,"4")==0; }
inline bool Enabled() { static const bool value=RangeRequested(std::getenv("BB_SUN_NEAR_FILTER_RANGE"));return value; }
enum class Selection { Native, PrecisionVS, UltraPair };
enum class Scope { None, Standard, HighUltra };
inline Scope ProfileScope(const char* profile,unsigned proxy_scale) {
    if(profile && std::strcmp(profile,"1")==0 && proxy_scale==0) return Scope::Standard;
    if(profile && std::strcmp(profile,"2")==0 && proxy_scale==2) return Scope::HighUltra;
    return Scope::None;
}
inline Scope CurrentScope(unsigned proxy_scale) {
    static const Scope scope=ProfileScope(std::getenv("BB_SUN_SHADOW_PROFILE"),proxy_scale);
    return scope;
}
inline bool RegistrationEnabled(Scope scope) { return scope!=Scope::None; }
inline Selection Select(bool candidate,bool ultra_requested,Scope scope) {
    return !candidate ? Selection::Native : ultra_requested && scope==Scope::HighUltra ? Selection::UltraPair : Selection::PrecisionVS;
}
inline bool Candidate(bool enabled,Scope scope,std::uint64_t vs,std::uint64_t ps,std::size_t count) {
    return enabled&&scope!=Scope::None&&vs==0xcb130cbfull&&ps==0xc6fa7896ull&&count==2;
}
template<std::size_t N> inline bool Exact(std::span<const std::uint32_t> code,const std::uint32_t(&native)[N]) {
    return code.size()==N&&std::memcmp(code.data(),native,sizeof(native))==0;
}
inline Kind Identify([[maybe_unused]] std::span<const std::uint32_t> code) {
#ifdef BB_SUN_NATIVE_PAYLOADS
    if(Exact(code,kUltraNativeVS)) return Kind::Vertex;
    if(Exact(code,kUltraNativePS)) return Kind::Fragment;
#endif
    return Kind::Unknown;
}
inline std::mutex registry_mutex;
inline std::map<ModuleKey,Kind> registry;
inline void Record(ModuleKey key,Kind kind) {
    std::lock_guard lock(registry_mutex);
    // Every incoming native/cache module is recreated through CompileSPV. Erase even
    // unknown streams so reuse cannot inherit authorization from a previous lifetime.
    registry.erase(key);
    if(kind!=Kind::Unknown) registry.emplace(key,kind);
}
inline bool KnownPair(ModuleKey vertex,ModuleKey fragment,std::size_t count) {
    if(count!=2||vertex.first!=fragment.first||vertex.second==fragment.second) return false;
    std::lock_guard lock(registry_mutex);
    const auto v=registry.find(vertex),f=registry.find(fragment);
    return v!=registry.end()&&f!=registry.end()&&v->second==Kind::Vertex&&f->second==Kind::Fragment;
}
}
