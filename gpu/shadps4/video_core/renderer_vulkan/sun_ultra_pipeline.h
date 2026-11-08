#pragma once
#include "sun_ultra_policy.h"
namespace BbSunUltra {
inline ModuleKey Key(vk::Device device,vk::ShaderModule module) {
    return {reinterpret_cast<std::uintptr_t>(static_cast<VkDevice>(device)),reinterpret_cast<std::uintptr_t>(static_cast<VkShaderModule>(module))};
}
inline void RecordModule(vk::Device device,vk::ShaderModule module,std::span<const std::uint32_t> code,Scope scope) {
    if(RegistrationEnabled(scope)) Record(Key(device,module),Identify(code));
}
template<class Stages> inline std::array<vk::UniqueShaderModule,2> ApplyPair(vk::Device device,Stages& stages,bool candidate) {
    std::array<vk::UniqueShaderModule,2> owned;
    if(!candidate||stages.size()!=2) return owned;
    vk::PipelineShaderStageCreateInfo *vertex=nullptr,*fragment=nullptr;
    for(auto& stage:stages) {
        if(stage.stage==vk::ShaderStageFlagBits::eVertex) vertex=&stage;
        else if(stage.stage==vk::ShaderStageFlagBits::eFragment) fragment=&stage;
        else return owned;
    }
    if(!vertex||!fragment||!KnownPair(Key(device,vertex->module),Key(device,fragment->module),stages.size())) return owned;
#ifdef BB_SUN_NATIVE_PAYLOADS
    auto create=[&](std::span<const std::uint32_t> code) {
        return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{.codeSize=code.size_bytes(),.pCode=code.data()});
    };
    auto v=create(kUltraVS);if(v.result!=vk::Result::eSuccess)return owned;
    auto f=create(kUltraPS);if(f.result!=vk::Result::eSuccess)return owned;
    // Both creations succeeded before either native stage is changed.
    owned[0]=std::move(v.value);owned[1]=std::move(f.value);
    vertex->module=*owned[0];fragment->module=*owned[1];
#endif
    return owned;
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
#ifdef BB_SUN_NATIVE_PAYLOADS
    auto v=device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
        .codeSize=sizeof(kNativeRayPrecisionVS),.pCode=kNativeRayPrecisionVS});
    if(v.result!=vk::Result::eSuccess) return owned;
    owned=std::move(v.value);vertex->module=*owned;
#endif
    return owned;
}
inline void ReportPrecisionActive(Scope) {}
inline void ReportActive() {}
}
