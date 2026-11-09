#pragma once
#include "sun_ultra_policy.h"
#include "main_sun_portable.h"
namespace BbSunUltra {
inline ModuleKey Key(vk::Device device,vk::ShaderModule module) {
    return {reinterpret_cast<std::uintptr_t>(static_cast<VkDevice>(device)),reinterpret_cast<std::uintptr_t>(static_cast<VkShaderModule>(module))};
}
inline void RecordModule(vk::Device device,vk::ShaderModule module,std::span<const std::uint32_t> code,Scope scope) {
    const auto key=Key(device,module);
    // Clear stale handle authority before any potentially failing preparation.
    Record(key,Kind::Unknown);
    if(!RegistrationEnabled(scope)) return;
    try {
        const auto kind=Identify(code);
        if(kind==Kind::Vertex) {
            MainSunPortable::Prepare(code,MainSunPortable::Variant::Precision);
            if(scope==Scope::Standard) MainSunPortable::Prepare(code,MainSunPortable::Variant::StandardVertex);
            else if(scope==Scope::HighUltra) MainSunPortable::Prepare(code,Enabled()?MainSunPortable::Variant::UltraVertex:MainSunPortable::Variant::HighVertex);
        } else if(kind==Kind::Fragment) {
            if(scope==Scope::Standard) MainSunPortable::Prepare(code,MainSunPortable::Variant::StandardFragment);
            else if(scope==Scope::HighUltra) MainSunPortable::Prepare(code,Enabled()?MainSunPortable::Variant::UltraFragment:MainSunPortable::Variant::HighFragment);
        }
        // Kind-only legacy authorization is published after synchronous preparation.
        Record(key,kind);
    } catch(...) {Record(key,Kind::Unknown);}
}
template<class Stages> inline std::array<vk::UniqueShaderModule,2> ApplyPair(vk::Device device,Stages& stages,bool candidate,Scope scope=Scope::None) {
    std::array<vk::UniqueShaderModule,2> owned;
    if(!candidate||stages.size()!=2) return owned;
    vk::PipelineShaderStageCreateInfo *vertex=nullptr,*fragment=nullptr;
    for(auto& stage:stages) {
        if(stage.stage==vk::ShaderStageFlagBits::eVertex) vertex=&stage;
        else if(stage.stage==vk::ShaderStageFlagBits::eFragment) fragment=&stage;
        else return owned;
    }
    if(!vertex||!fragment||!KnownPair(Key(device,vertex->module),Key(device,fragment->module),stages.size())) return owned;
    auto create=[&](std::span<const std::uint32_t> code) {
        return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{.codeSize=code.size_bytes(),.pCode=code.data()});
    };
    const auto vs=MainSunPortable::Get(scope==Scope::Standard?MainSunPortable::Variant::StandardVertex:Enabled()?MainSunPortable::Variant::UltraVertex:MainSunPortable::Variant::HighVertex);
    const auto ps=MainSunPortable::Get(scope==Scope::Standard?MainSunPortable::Variant::StandardFragment:Enabled()?MainSunPortable::Variant::UltraFragment:MainSunPortable::Variant::HighFragment);
    if(!vs||!ps) return owned;
    auto v=create(*vs);if(v.result!=vk::Result::eSuccess)return owned;
    auto f=create(*ps);if(f.result!=vk::Result::eSuccess)return owned;
    // Both creations succeeded before either native stage is changed.
    owned[0]=std::move(v.value);owned[1]=std::move(f.value);
    vertex->module=*owned[0];fragment->module=*owned[1];return owned;
}
// Native High changes ONLY the VS module; original PS identity is still required
// to prove the incoming shared layout, and its handle remains untouched.
template<class Stages> inline vk::UniqueShaderModule ApplyVertex(vk::Device device,Stages& stages,bool candidate) {
    vk::UniqueShaderModule owned;
    if(!candidate||stages.size()!=2) return owned;
    vk::PipelineShaderStageCreateInfo *vertex=nullptr,*fragment=nullptr;
    for(auto& stage:stages) {
        if(stage.stage==vk::ShaderStageFlagBits::eVertex) vertex=&stage;
        else if(stage.stage==vk::ShaderStageFlagBits::eFragment) fragment=&stage;
        else return owned;
    }
    if(!vertex||!fragment||!KnownPair(Key(device,vertex->module),Key(device,fragment->module),stages.size())) return owned;
    const auto code=MainSunPortable::Get(MainSunPortable::Variant::Precision);
    if(!code) return owned;
    auto v=device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
        .codeSize=code->size()*sizeof(std::uint32_t),.pCode=code->data()});
    if(v.result!=vk::Result::eSuccess) return owned;
    owned=std::move(v.value);vertex->module=*owned;return owned;
}
inline void ReportPrecisionActive(Scope scope) {
    static std::atomic_flag reported=ATOMIC_FLAG_INIT;
    if(!reported.test_and_set()) {
        std::printf("SUN_NATIVE_RAY_PRECISION: active mode=%s fullXYZ=1 proxy_scale=%u VS=cb130cbf PS=c6fa7896 native_PS=unchanged portable_derived=1\n",scope==Scope::Standard?"Standard":"High",scope==Scope::Standard?0u:2u);
        std::fflush(stdout);
    }
}
inline void ReportActive(Scope scope=Scope::HighUltra) {
    static std::atomic_flag reported=ATOMIC_FLAG_INIT;
    if(!reported.test_and_set()) {
        if(scope==Scope::Standard) {
            std::printf("SUN_STANDARD_FILTER: active private_phase=shared-far-floor-G correction_reach_min_m=100 near_policy=unchanged profile=1 proxy_scale=0 expected_atlas=9216x8192 pair=VS+PS receiver_plane=full_chosen radius=publisher_near_texel_min C0_radius=native portable_derived=1\n");
            std::fflush(stdout);
            return;
        }
        if(!Enabled()) {std::printf("SUN_NATIVE_HIGH: paired Ultra-near-filter transfer E2 with shared-far-floor-G active; correction_reach_min_m=100 near_policy=unchanged; C0_radius=Ultra_guest_geometry C123_reference=Ultra_guest_geometry full_reference=1 range50=1; atlas=14336x8192 unproxied=1 layout=high-intermediate-v1; portable_derived=1\n");std::fflush(stdout);return;}
        std::printf("SUN_NATIVE_ULTRA: paired shared-C full-reference near-texel range50 D active; atlas=18944x21504; VS=cb130cbf PS=c6fa7896; ray_precision=fullXYZ; portable_derived=1\n");
        std::fflush(stdout);
    }
}
}
