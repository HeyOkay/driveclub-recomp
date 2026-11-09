#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
namespace Vulkan::NativeDepthGrid {
struct Key {
    std::uint64_t uid{};std::uintptr_t native{},backing{};std::uint32_t width{},height{};
    bool operator==(const Key&) const=default;
    bool Physical(const Key& k) const {return (native&&native==k.native)||(backing&&backing==k.backing);}
    bool Valid() const {return uid&&native&&backing&&width&&height;}
};
inline bool Supported(std::array<float,2> j) {
    return std::isfinite(j[0])&&std::isfinite(j[1])&&std::abs(j[0])<=0.5f&&std::abs(j[1])<=0.5f;
}
inline std::optional<std::array<float,2>> EffectiveJ(float x,float y,float w,float h,
                                                   std::uint32_t width,std::uint32_t height) {
    if(!width||!height||w!=float(width)||h!=-float(height))return {};
    const std::array<float,2> j{x,y-float(height)};
    if(!Supported(j))return {};return j;
}
inline bool SmallWorldProducer(bool scope,std::uint64_t vertex_shader,bool clip_disabled,
                               bool depth_test,bool depth_write,bool clear_mode,unsigned attributes) {
    return scope&&vertex_shader==0x03528cbaull&&!clip_disabled&&depth_test&&depth_write&&!clear_mode&&attributes>0;
}
struct Tracking {
    std::uintptr_t runtime{};std::uint64_t session{},frame=1;Key source{};
    bool valid=false,blocked=false,initialized=false;std::array<float,2> jitter{};
    std::array<Key,16> clears{};unsigned clear_count=0;
    bool operator==(const Tracking&) const=default;
    bool WasCleared(const Key& k) const {for(unsigned i=0;i<clear_count;++i)if(clears[i]==k)return true;return false;}
    void Clear(const Key& k) {
        if(!k.Valid())return;
        for(unsigned i=0;i<clear_count;) {
            if(clears[i].Physical(k))clears[i]=clears[--clear_count];else ++i;
        }
        if(clear_count<clears.size())clears[clear_count++]=k;
        if(source.Physical(k)){source=k;valid=blocked=false;initialized=WasCleared(k);jitter={};}
    }
    void NextFrame(){++frame;source={};valid=blocked=initialized=false;jitter={};clear_count=0;}
    void Write(const Key& k,bool attachment_write) {
        if(attachment_write)return; // transition intent is not an actual draw/write
        for(unsigned i=0;i<clear_count;) {
            if(clears[i].Physical(k))clears[i]=clears[--clear_count];else ++i;
        }
        if(source.Physical(k)){valid=false;blocked=true;initialized=false;}
    }
    void Draw(const Key& k,bool main_scope,bool qualified,bool writes,bool clear,std::array<float,2> j) {
        if(!writes&&!clear)return;
        if(!main_scope && source!=k){if(writes)Write(k,false);return;}
        if(writes)for(unsigned i=0;i<clear_count;) {
            if(clears[i]!=k && clears[i].Physical(k))clears[i]=clears[--clear_count];else ++i;
        }
        if(source!=k && source.Physical(k))Write(k,false);
        if(source!=k){source=k;valid=blocked=false;initialized=WasCleared(k);}
        if(clear)Clear(k);
        if(!writes)return;
        if(!qualified||!k.Valid()||!Supported(j)||(j[0]==0.f&&j[1]==0.f)){Write(k,false);valid=false;blocked=true;return;}
        if(valid&&jitter!=j){Write(k,false);valid=false;blocked=true;return;}
        if(!initialized){blocked=true;return;}
        if(!blocked){jitter=j;valid=true;}
    }
    std::optional<std::array<float,2>> Query(std::uintptr_t owner,const Key& k,bool enabled) const {
        if(!enabled||!owner||runtime!=owner||!session||!frame||!valid||blocked||source!=k||!k.Valid()||!Supported(jitter)||
           (jitter[0]==0.f&&jitter[1]==0.f))return {};
        return jitter;
    }
};
}
