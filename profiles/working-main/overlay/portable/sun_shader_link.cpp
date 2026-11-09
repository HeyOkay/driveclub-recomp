// SPDX-License-Identifier: GPL-2.0-or-later
#include "sun_shader_link.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/linker.hpp>
#include <spirv/unified1/spirv.hpp11>

namespace BbSunPortable {
namespace {
using W=std::uint32_t;
using Words=std::vector<W>;
struct Inst { W op{}, result{}, type{}; Words a; };
[[noreturn]] void Fail(const std::string& text) { throw std::runtime_error("Sun helper linkage: "+text); }
std::string String(const Words& a,size_t first) {
    if(first>=a.size()) Fail("missing string");
    const char* p=reinterpret_cast<const char*>(a.data()+first);
    const size_t n=(a.size()-first)*4;
    const auto end=static_cast<const char*>(std::memchr(p,0,n));
    if(!end) Fail("unterminated string");
    return {p,end};
}
Words Text(const std::string& s) { Words w((s.size()+4)/4);std::memcpy(w.data(),s.c_str(),s.size());return w; }
Inst Make(spv::Op op,Words a) { return {W(op),0,0,std::move(a)}; }
void Append(Words& out,const Inst& i) { out.push_back((W(i.a.size()+1)<<16)|i.op);out.insert(out.end(),i.a.begin(),i.a.end()); }
struct Module {
    Words header;std::vector<Inst> ins;std::map<W,Inst> defs;std::map<W,std::string> names;
    explicit Module(std::span<const W> words) {
        if(words.size()<5||words[0]!=0x07230203u||(words[1]<0x00010500u||words[1]>0x00010600u)||words[3]==0||words[3]>0x3fffffu) Fail("unsupported SPIR-V header");
        header.assign(words.begin(),words.begin()+5);
        spv_context context=spvContextCreate(SPV_ENV_UNIVERSAL_1_6);
        auto cb=[](void* u,const spv_parsed_instruction_t* p)->spv_result_t {
            auto& m=*static_cast<Module*>(u);Inst i{p->opcode,p->result_id,p->type_id,{p->words+1,p->words+p->num_words}};
            m.ins.push_back(i);
            if(i.result&&!m.defs.emplace(i.result,i).second)return SPV_ERROR_INVALID_ID;
            return SPV_SUCCESS;
        };
        spv_diagnostic diagnostic=nullptr;auto status=spvBinaryParse(context,this,words.data(),words.size(),nullptr,cb,&diagnostic);
        std::string message=diagnostic&&diagnostic->error?diagnostic->error:"malformed instruction stream";
        spvDiagnosticDestroy(diagnostic);spvContextDestroy(context);
        if(status!=SPV_SUCCESS) Fail(message);
        for(const auto& i:ins)if(i.op==W(spv::Op::OpName))names[i.a[0]]=String(i.a,1);
    }
    Words Write()const { Words out=header;for(const auto& i:ins)Append(out,i);return out; }
    W Id() { if(header[3]>=0x3fffffu)Fail("ID limit");return header[3]++; }
    const Inst& Def(W id)const { auto it=defs.find(id);if(it==defs.end())Fail("missing definition");return it->second; }
    std::vector<Inst> Decorations(W id)const {
        std::vector<Inst> v;for(const auto&i:ins)if((i.op==W(spv::Op::OpDecorate)||i.op==W(spv::Op::OpMemberDecorate))&&i.a[0]==id)v.push_back(i);return v;
    }
    W Decoration(W id,spv::Decoration kind,W fallback=~W(0))const {
        W value=fallback;for(const auto&i:ins)if(i.op==W(spv::Op::OpDecorate)&&i.a[0]==id&&i.a[1]==W(kind)){
            if(i.a.size()!=3||value!=fallback)Fail("ambiguous decoration");value=i.a[2];
        }
        return value;
    }
    // Semantic type shape including physical layout, independent of numeric IDs.
    std::string Shape(W id,unsigned depth=0)const {
        if(depth>32)Fail("recursive type");const auto&d=Def(id);std::string s=std::to_string(d.op)+"(";
        const auto add=[&](W x){s+=Shape(x,depth+1)+",";};
        if(d.op==W(spv::Op::OpTypePointer)){s+=std::to_string(d.a[1])+",";add(d.a[2]);}
        else if(d.op==W(spv::Op::OpTypeVector)||d.op==W(spv::Op::OpTypeArray)){add(d.a[1]);if(d.op==W(spv::Op::OpTypeArray))add(d.a[2]);else s+=std::to_string(d.a[2]);}
        else if(d.op==W(spv::Op::OpTypeRuntimeArray)||d.op==W(spv::Op::OpTypeSampledImage)){add(d.a[1]);}
        else if(d.op==W(spv::Op::OpTypeStruct)){for(size_t j=1;j<d.a.size();++j)add(d.a[j]);}
        else if(d.op==W(spv::Op::OpTypeImage)){add(d.a[1]);for(size_t j=2;j<d.a.size();++j)s+=std::to_string(d.a[j])+",";}
        else if(d.op==W(spv::Op::OpConstant)){add(d.a[0]);for(size_t j=2;j<d.a.size();++j)s+=std::to_string(d.a[j])+",";}
        else {for(size_t j=1;j<d.a.size();++j)s+=std::to_string(d.a[j])+",";}
        for(const auto&i:Decorations(id)){
            if(i.op==W(spv::Op::OpDecorate)&&(i.a[1]==W(spv::Decoration::ArrayStride)||i.a[1]==W(spv::Decoration::Block))){for(size_t j=1;j<i.a.size();++j)s+="d"+std::to_string(i.a[j]);}
            if(i.op==W(spv::Op::OpMemberDecorate)&&i.a[2]==W(spv::Decoration::Offset)){for(size_t j=1;j<i.a.size();++j)s+="m"+std::to_string(i.a[j]);}
        }return s+")";
    }
};
Inst Linkage(W id,const std::string& name,spv::LinkageType kind) {
    Words a{id,W(spv::Decoration::LinkageAttributes)};auto s=Text(name);a.insert(a.end(),s.begin(),s.end());a.push_back(W(kind));return Make(spv::Op::OpDecorate,std::move(a));
}
void AddAnnotations(Module& m,const std::vector<Inst>& annotations) {
    auto at=std::find_if(m.ins.begin(),m.ins.end(),[](const Inst&i){return i.op>=W(spv::Op::OpTypeVoid)&&i.op<=W(spv::Op::OpTypeForwardPointer);});
    m.ins.insert(at,annotations.begin(),annotations.end());
}
void AddDeclarations(Module& m,const std::vector<Inst>& declarations) {
    auto at=std::find_if(m.ins.begin(),m.ins.end(),[](const Inst&i){return i.op==W(spv::Op::OpFunction);});
    m.ins.insert(at,declarations.begin(),declarations.end());
}
void AddLinkage(Module&m){m.ins.insert(m.ins.begin(),Make(spv::Op::OpCapability,{W(spv::Capability::Linkage)}));}
}
W LinkPlan::AllocateId(){if(native.size()<5||native[3]>=0x3fffffu)Fail("ID limit");return native[3]++;}
LinkPlan PrepareLink(std::span<const W> native,std::span<const W> authored_helper) {
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_3);
    if(!validator.Validate(native.data(),native.size()))Fail("invalid native input");
    if(!validator.Validate(authored_helper.data(),authored_helper.size()))Fail("invalid authored helper input");
    Module n(native),h(authored_helper);LinkPlan plan;std::vector<Inst> na,ha,decl,prototypes;
    std::set<W> removed;W main=0;
    for(const auto&i:h.ins)if(i.op==W(spv::Op::OpEntryPoint)){if(main)Fail("multiple helper entry points");main=i.a[1];}
    if(!main)Fail("missing helper main");
    bool in_main=false;
    for(const auto&i:h.ins){if(i.op==W(spv::Op::OpFunction))in_main=i.result==main;if(in_main&&i.result)removed.insert(i.result);if(i.op==W(spv::Op::OpFunctionEnd))in_main=false;}
    in_main=false;std::vector<Inst> kept;
    for(const auto&i:h.ins){
        if(i.op==W(spv::Op::OpFunction))in_main=i.result==main;
        bool drop=in_main||i.op==W(spv::Op::OpEntryPoint)||i.op==W(spv::Op::OpExecutionMode)||i.op==W(spv::Op::OpExecutionModeId);
        if((i.op==W(spv::Op::OpName)||i.op==W(spv::Op::OpDecorate))&&!i.a.empty()&&removed.contains(i.a[0]))drop=true;
        // glslang adds member NonWritable while native readonly is on the variable.
        if(i.op==W(spv::Op::OpMemberDecorate)&&i.a[2]==W(spv::Decoration::NonWritable))drop=true;
        if(!drop)kept.push_back(i);if(i.op==W(spv::Op::OpFunctionEnd))in_main=false;
    }h.ins=std::move(kept);
    // The native emitter declares sampled 2D images with Depth=0, including
    // comparison images; glslang infers Depth=1 from sampler2DShadow. The
    // authored helper only queries that image's size. Match the native type
    // declaration without touching any native instruction or sampler operation.
    for(auto& i:h.ins)if(i.op==W(spv::Op::OpTypeImage)&&i.a.size()==8&&i.a[2]==W(spv::Dim::Dim2D)&&i.a[3]==1&&i.a[6]==1) {
        for(const auto&[id,d]:n.defs)if(d.op==i.op&&d.a.size()==8&&d.a[2]==i.a[2]&&d.a[3]==0&&d.a[4]==i.a[4]&&d.a[5]==i.a[5]&&d.a[6]==i.a[6]&&d.a[7]==i.a[7]&&n.Shape(d.a[1])==h.Shape(i.a[1])) {
            i.a[3]=0;h.defs[i.result]=i;break;
        }
    }
    W native_void=0,native_voidfn=0;
    for(const auto&[id,d]:n.defs)if(d.op==W(spv::Op::OpTypeVoid))native_void=id;
    for(const auto&[id,d]:n.defs)if(d.op==W(spv::Op::OpTypeFunction)&&d.a.size()==2&&d.a[1]==native_void)native_voidfn=id;
    if(!native_void||!native_voidfn)Fail("missing void function type");
    auto value_type=[&](W helper_type){
        const auto shape=h.Shape(helper_type);for(const auto&[id,d]:n.defs)if(d.op>=W(spv::Op::OpTypeVoid)&&d.op<=W(spv::Op::OpTypeForwardPointer)&&n.Shape(id)==shape)return id;
        Fail("private ABI type absent in native module");
    };
    const auto helper_instructions=h.ins;
    for(const auto&i:helper_instructions){
        if(i.op==W(spv::Op::OpFunction)&&i.result!=main){
            auto name=h.names[i.result];auto p=name.find('(');if(p!=name.npos)name.resize(p);
            if(name!="portableVertex"&&name!="portablePrepare"&&name!="portableReference")continue;
            const auto& ft=h.Def(i.a[3]);if(i.type==0||h.Def(i.type).op!=W(spv::Op::OpTypeVoid)||ft.a.size()!=2)Fail("unsupported helper function ABI");
            W id=n.Id();prototypes.push_back(Make(spv::Op::OpFunction,{native_void,id,0,native_voidfn}));prototypes.push_back(Make(spv::Op::OpFunctionEnd,{}));
            na.push_back(Linkage(id,name,spv::LinkageType::Import));ha.push_back(Linkage(i.result,name,spv::LinkageType::Export));
            if(!plan.symbols.emplace(name,LinkedSymbol{id,native_void}).second)Fail("duplicate function export");
        }
        if(i.op!=W(spv::Op::OpVariable)||i.a[2]==W(spv::StorageClass::Function))continue;
        const W storage=i.a[2];const std::string name=h.names[i.result];
        if(storage==W(spv::StorageClass::Private)){
            if(!name.starts_with("portable_"))continue;
            const W type=value_type(h.Def(i.type).a[2]);W ptr=0;
            for(const auto&[id,d]:n.defs)if(d.op==W(spv::Op::OpTypePointer)&&d.a[1]==storage&&d.a[2]==type)ptr=id;
            if(!ptr){ptr=n.Id();decl.push_back(Make(spv::Op::OpTypePointer,{ptr,storage,type}));}
            W id=n.Id();decl.push_back(Make(spv::Op::OpVariable,{ptr,id,storage}));
            na.push_back(Linkage(id,name,spv::LinkageType::Import));ha.push_back(Linkage(i.result,name,spv::LinkageType::Export));
            if(!plan.symbols.emplace(name,LinkedSymbol{id,type}).second)Fail("duplicate private export");continue;
        }
        bool shared=storage==W(spv::StorageClass::UniformConstant)||storage==W(spv::StorageClass::StorageBuffer)||storage==W(spv::StorageClass::PushConstant);
        if(storage==W(spv::StorageClass::Input)&&h.Decoration(i.result,spv::Decoration::Location)<3)shared=true;
        if(!shared)continue;
        W found=0;
        for(const auto&[id,d]:n.defs){
            if(d.op!=W(spv::Op::OpVariable)||d.a[2]!=storage||n.Shape(d.type)!=h.Shape(i.type))continue;
            bool match=true;
            for(auto decoration:{spv::Decoration::DescriptorSet,spv::Decoration::Binding,spv::Decoration::Location,spv::Decoration::BuiltIn})if(n.Decoration(id,decoration)!=h.Decoration(i.result,decoration))match=false;
            if(match){if(found)Fail("ambiguous shared global");found=id;}
        }
        if(!found)Fail("unmatched helper global "+name);
        const auto symbol="sun_native_"+std::to_string(found);na.push_back(Linkage(found,symbol,spv::LinkageType::Export));ha.push_back(Linkage(i.result,symbol,spv::LinkageType::Import));
        // The linker's ABI checks require matching direct decorations, including readonly.
        h.ins.erase(std::remove_if(h.ins.begin(),h.ins.end(),[&](const Inst&d){return d.op==W(spv::Op::OpDecorate)&&d.a[0]==i.result;}),h.ins.end());
        for(auto d:n.Decorations(found))if(d.op==W(spv::Op::OpDecorate)){d.a[0]=i.result;ha.push_back(d);}
    }
    if(plan.symbols.empty())Fail("empty helper ABI");
    AddLinkage(n);AddLinkage(h);AddAnnotations(n,na);AddAnnotations(h,ha);decl.insert(decl.end(),prototypes.begin(),prototypes.end());AddDeclarations(n,decl);
    plan.native=n.Write();plan.library=h.Write();return plan;
}
std::vector<W> FinishLink(LinkPlan plan) {
    spvtools::Context context(SPV_ENV_UNIVERSAL_1_6);std::string diagnostic;
    context.SetMessageConsumer([&](spv_message_level_t,const char*,const spv_position_t&,const char* message){diagnostic+=message;});
    spvtools::LinkerOptions options;options.SetVerifyIds(true);options.SetUseHighestVersion(true);options.SetAllowPartialLinkage(false);
    Words output;if(spvtools::Link(context,{plan.native,plan.library},&output,options)!=SPV_SUCCESS)Fail(diagnostic);
    Module linked(output);std::set<W> globals;
    for(const auto&i:linked.ins){
        if(i.op==W(spv::Op::OpCapability)&&i.a[0]==W(spv::Capability::Linkage))Fail("unresolved linkage capability");
        if(i.op==W(spv::Op::OpVariable)&&i.a[2]!=W(spv::StorageClass::Function))globals.insert(i.result);
    }
    for(auto&i:linked.ins)if(i.op==W(spv::Op::OpEntryPoint)){
        const auto name=String(i.a,2);const size_t first=2+(name.size()+4)/4;std::set<W> have(i.a.begin()+first,i.a.end());
        for(W id:globals)if(!have.contains(id))i.a.push_back(id);
    }
    output=linked.Write();spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_3);
    tools.SetMessageConsumer([&](spv_message_level_t,const char*,const spv_position_t&,const char* message){diagnostic+=message;});
    if(!tools.Validate(output))Fail(diagnostic);return output;
}
}
