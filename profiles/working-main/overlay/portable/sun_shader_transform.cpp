// SPDX-License-Identifier: GPL-2.0-or-later
#include "sun_shader_transform.h"
#include "sun_shader_precision.h"
#include "sun_shader_link.h"
#include "video_core/host_shaders/sun_portable_vertex.h"
#include "video_core/host_shaders/sun_portable_fragment.h"
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace BbSunPortable {
namespace {
using Words=std::vector<std::uint32_t>;
// These are sparse contracts for the exact SHA-gated native modules, never a
// serialized native program. Validate instruction boundaries before locating IDs.
struct Instruction {std::size_t offset;std::uint32_t count,opcode;};
std::vector<Instruction> Decode(std::span<const std::uint32_t> code) {
    if(code.size()<5||code[0]!=0x07230203||code[1]!=0x00010600||code[4]!=0) throw std::runtime_error("sun native header");
    std::vector<Instruction> result;
    for(std::size_t i=5;i<code.size();) {
        const auto count=code[i]>>16,opcode=code[i]&0xffff;
        if(count==0||count>code.size()-i) throw std::runtime_error("sun native instruction");
        result.push_back({i,count,opcode});i+=count;
    }
    return result;
}
Instruction Require(const Words& code,std::uint32_t opcode,std::uint32_t type,std::uint32_t id,
                    std::initializer_list<std::uint32_t> operands={}) {
    Instruction result{};bool found=false;
    for(auto inst:Decode(code)) {
        if(inst.opcode!=opcode||inst.count<3||code[inst.offset+2]!=id) continue;
        if(found||code[inst.offset+1]!=type||(operands.size()!=0 && inst.count!=operands.size()+3))
            throw std::runtime_error("sun native SSA shape");
        std::size_t j=3;for(auto operand:operands) if(code[inst.offset+j++]!=operand) throw std::runtime_error("sun native SSA operand");
        result=inst;found=true;
    }
    if(!found) throw std::runtime_error("sun native SSA missing");
    return result;
}
LinkedSymbol Symbol(const LinkPlan& plan,const char* name,std::uint32_t type) {
    const auto it=plan.symbols.find(name);
    if(it==plan.symbols.end()||it->second.type!=type||it->second.id==0) throw std::runtime_error("sun helper ABI");
    return it->second;
}
void Emit(Words& code,std::uint32_t opcode,std::initializer_list<std::uint32_t> operands) {
    code.push_back((static_cast<std::uint32_t>(operands.size()+1)<<16)|opcode);
    code.insert(code.end(),operands);
}
void Store(Words& code,const LinkPlan& plan,const char* name,std::uint32_t type,std::uint32_t value) {
    Emit(code,62,{Symbol(plan,name,type).id,value});
}
std::uint32_t Load(Words& code,LinkPlan& plan,const char* name,std::uint32_t type) {
    const auto id=plan.AllocateId();Emit(code,61,{type,id,Symbol(plan,name,type).id});return id;
}
void Call(Words& code,LinkPlan& plan,const char* name) {
    const auto id=plan.AllocateId();Emit(code,57,{2,id,Symbol(plan,name,2).id});
}
std::uint32_t Composite(Words& code,LinkPlan& plan,std::uint32_t type,std::initializer_list<std::uint32_t> components) {
    const auto id=plan.AllocateId();code.push_back((static_cast<std::uint32_t>(components.size()+3)<<16)|80);
    code.push_back(type);code.push_back(id);code.insert(code.end(),components);return id;
}
std::uint32_t BitcastFloat(Words& code,LinkPlan& plan,std::uint32_t value) {
    const auto id=plan.AllocateId();Emit(code,124,{8,id,value});return id;
}
std::uint32_t ExtractFloat(Words& code,LinkPlan& plan,std::uint32_t composite,std::uint32_t component) {
    const auto id=plan.AllocateId();Emit(code,81,{8,id,composite,component});return id;
}
void InsertAll(Words& code,const std::map<std::size_t,Words>& before) {
    Words result;result.reserve(code.size()+512);
    std::size_t cursor=0;
    for(const auto& [position,insertion]:before) {
        if(position<cursor||position>code.size()) throw std::runtime_error("sun insertion order");
        result.insert(result.end(),code.begin()+cursor,code.begin()+position);
        result.insert(result.end(),insertion.begin(),insertion.end());cursor=position;
    }
    result.insert(result.end(),code.begin()+cursor,code.end());code=std::move(result);
}
void FragmentContracts(const Words& code) {
    Require(code,80,12,169,{165,168});Require(code,124,8,163,{129});Require(code,124,8,166,{131});
    Require(code,81,8,174,{173,0});Require(code,124,8,274,{178});Require(code,136,8,278,{277,276});
    Require(code,12,8,235,{141,50,234,101,188});Require(code,12,8,243,{141,50,242,101,229});
    Require(code,12,8,257,{141,50,256,101,251});Require(code,245,22,216,{203,199,215,200});
    Require(code,61,10,267,{266});Require(code,61,10,271,{270});
    Require(code,129,8,285,{284,272});Require(code,129,8,288,{287,273});Require(code,61,10,263,{262});
    Require(code,129,8,290,{289,259});Require(code,124,8,307,{300});Require(code,124,8,306,{304});
    Require(code,133,8,308,{307,305});Require(code,133,8,311,{310,306});
    Require(code,90,8,318,{317,314,290,2,31});
}
} // namespace

std::vector<std::uint32_t> RewriteUltraVertex(std::span<const std::uint32_t> native,std::span<const std::uint32_t> helper) {
    if(!BbSunPrecision::IsNativeVertex(native)) return {};
    try {
        auto precise=BbSunPrecision::RewriteVertex(native);if(precise.empty()) return {};
        Require(precise,194,10,103,{102,93}); // Native bound global-buffer dword offset.
        auto plan=PrepareLink(precise,helper);
        // The exact module has one main and one terminal return. Inserting here
        // leaves every native CFG edge and all original output stores intact.
        std::size_t return_offset=0;std::size_t returns=0;
        for(auto inst:Decode(plan.native)) if(inst.opcode==253) {return_offset=inst.offset;++returns;}
        if(returns!=1) throw std::runtime_error("sun vertex return");
        Words append;Store(append,plan,"portable_buf4off",10,103);Call(append,plan,"portableVertex");
        InsertAll(plan.native,{{return_offset,std::move(append)}});
        return FinishLink(std::move(plan));
    } catch(...) {return {};}
}
std::vector<std::uint32_t> RewriteUltraFragment(std::span<const std::uint32_t> native,std::span<const std::uint32_t> helper) {
    if(!BbSunPrecision::IsNativeFragment(native)) return {};
    try {
        Words original(native.begin(),native.end());FragmentContracts(original);
        auto plan=PrepareLink(native,helper);FragmentContracts(plan.native);
        const auto ref=Require(plan.native,129,8,290,{289,259});
        const auto tap_x=Require(plan.native,133,8,308,{307,305});
        const auto tap_y=Require(plan.native,133,8,311,{310,306});
        const auto sample=Require(plan.native,90,8,318,{317,314,290,2,31});
        Words prepare;
        Store(prepare,plan,"portable_depthUV",12,169);
        Store(prepare,plan,"portable_cameraUVScale",12,Composite(prepare,plan,12,{163,166}));
        Store(prepare,plan,"portable_nativeDepth",8,174);Store(prepare,plan,"portable_cameraA",8,274);
        Store(prepare,plan,"portable_viewZ",8,278);Store(prepare,plan,"portable_ray",16,Composite(prepare,plan,16,{235,243,257}));
        Store(prepare,plan,"portable_affine",22,216);
        const auto rx=BitcastFloat(prepare,plan,267),ry=BitcastFloat(prepare,plan,271);
        Store(prepare,plan,"portable_nativeRadius",12,Composite(prepare,plan,12,{rx,ry}));
        Store(prepare,plan,"portable_center",12,Composite(prepare,plan,12,{285,288}));
        Store(prepare,plan,"portable_count",10,263);Store(prepare,plan,"portable_reference",8,290);
        Call(prepare,plan,"portablePrepare");
        const auto radius=Load(prepare,plan,"portable_effectiveRadius",12);
        const auto effective_x=ExtractFloat(prepare,plan,radius,0),effective_y=ExtractFloat(prepare,plan,radius,1);
        Words per_tap;
        Store(per_tap,plan,"portable_tap",12,Composite(per_tap,plan,12,{307,306}));Call(per_tap,plan,"portableReference");
        const auto corrected=Load(per_tap,plan,"portable_correctedReference",8);
        // Keep every original native instruction, only replacing three operands.
        // Native count, loop Phi nodes, reduction and fade remain untouched.
        plan.native[tap_x.offset+4]=effective_x;plan.native[tap_y.offset+3]=effective_y;
        plan.native[sample.offset+5]=corrected;
        InsertAll(plan.native,{{ref.offset+ref.count,std::move(prepare)},{sample.offset,std::move(per_tap)}});
        return FinishLink(std::move(plan));
    } catch(...) {return {};}
}
std::vector<std::uint32_t> RewriteUltraVertex(std::span<const std::uint32_t> native) {
    return RewriteUltraVertex(native,std::span{SUN_PORTABLE_VERTEX});
}
std::vector<std::uint32_t> RewriteUltraFragment(std::span<const std::uint32_t> native) {
    return RewriteUltraFragment(native,std::span{SUN_PORTABLE_FRAGMENT});
}
} // namespace BbSunPortable
