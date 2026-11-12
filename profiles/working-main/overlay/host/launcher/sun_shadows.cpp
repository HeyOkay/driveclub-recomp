#include "sun_shadows.h"
#include "rear_native_payload.h"
#include "native_runtime_identity.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<unsigned char>;
constexpr char kSunVersion[]="sun-shadows-pcss-v3-rear-native-allocation";
constexpr char kUltraVersion[]="sun-shadows-native-ultra-v1-near-range4";
constexpr char kHighVersion[]="sun-shadows-native-v6-intermediate-14336x8192-layout1";
constexpr char kRuntimeMarker[]="BB_SUN_ATLAS_PROXY_SUMMARY:";

[[noreturn]] void Fail(const char* message) { throw std::runtime_error(message); }
Bytes Read(const fs::path& p) {
  std::ifstream f(p,std::ios::binary);if(!f)Fail("A required sun shadow source file could not be read.");
  return Bytes(std::istreambuf_iterator<char>(f),{});
}
void Write(const fs::path& p,const Bytes& bytes) {
  std::ofstream f(p,std::ios::binary|std::ios::trunc);
  if(!f || (!bytes.empty()&&!f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size())))
    Fail("The sun shadow cache could not be written.");
  f.close();if(!f)Fail("The sun shadow cache could not be closed.");
}
std::string Hash(const Bytes& bytes) {
  BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
  if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)Fail("SHA-256 is unavailable.");
  DWORD n=0,got=0;Bytes digest(32);
  if(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&n),sizeof n,&got,0)<0) {
    BCryptCloseAlgorithmProvider(alg,0);Fail("SHA-256 initialization failed.");
  }
  Bytes object(n);
  bool ok=BCryptCreateHash(alg,&hash,object.data(),n,nullptr,0,0)>=0;
  if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0)>=0;
  if(ok)ok=BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0;
  if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
  if(!ok)Fail("SHA-256 verification failed.");
  std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);
  return out.str();
}
Bytes Text(const std::string& s) { return Bytes(s.begin(),s.end()); }
template<class T> T Get(const Bytes& b,size_t at) {
  if(at>b.size()||sizeof(T)>b.size()-at)Fail("A sun shadow source file is truncated.");
  T value;std::memcpy(&value,b.data()+at,sizeof value);return value;
}
template<class T> void Set(Bytes& b,size_t at,T value) {
  if(at>b.size()||sizeof(T)>b.size()-at)Fail("A sun shadow patch is out of bounds.");
  std::memcpy(b.data()+at,&value,sizeof value);
}
template<class T> void Append(Bytes& b,T value) {
  const auto p=reinterpret_cast<const unsigned char*>(&value);b.insert(b.end(),p,p+sizeof value);
}
Bytes Hex(const char* s) {
  Bytes out;while(*s) {unsigned x=0;for(int i=0;i<2;++i){char c=*s++;x=x*16+(c>='a'?c-'a'+10:c-'0');}out.push_back((unsigned char)x);}return out;
}
void Expect(const Bytes& b,size_t at,const Bytes& sig) {
  if(at>b.size()||sig.size()>b.size()-at||!std::equal(sig.begin(),sig.end(),b.begin()+at))
    Fail("This prepared game is not supported by Sun shadows. Select Standard or prepare the supported game build again.");
}
bool SupportsRuntime(const fs::path& path) {
  std::ifstream f(path,std::ios::binary);std::string tail,chunk(65536,'\0');
  while(f) {f.read(chunk.data(),chunk.size());const auto count=size_t(f.gcount());
    auto part=tail+chunk.substr(0,count);if(part.find(kRuntimeMarker)!=std::string::npos)return true;
    tail=part.substr(part.size()>(sizeof kRuntimeMarker)?part.size()-sizeof kRuntimeMarker:0);
  }return false;
}
// The verified EU1.28 image differs from US1.28 only in53 regional-label bytes.
// Keep exact whole-image identities at every stage; unknown or modified images
// must still fail before any patch is applied. Cache hashes remain unmodified.
bool MatchesBootHash(const Bytes& boot,const char* us,const char* eu) {
  const auto digest=Hash(boot);return digest==us || digest==eu;
}
void PatchBoot(Bytes& boot) {
  if(!MatchesBootHash(boot,"839d2995699c9179d6510ba773702b47a44eac8196038f23c27abe2ce3cbb971","cb2ae1d90e240bde81ff2bea048c4eb16a9287185afd05fa36fb32e4e7c88156"))
    Fail("Sun shadows require a verified US or European 1.28 prepared image. Select Standard or prepare the supported build again.");
  Expect(boot,0,Text("BBPROBE6"));const auto size=Get<uint64_t>(boot,8);
  if(size>boot.size())Fail("Invalid prepared image size.");const auto image=boot.size()-size;
  if(image!=0x348180)Fail("Unsupported prepared image layout.");
  struct Site {size_t va;const char* sig;uint32_t w,h;};
  const Site sites[]={
    {0x5b63eb,"ba00080000b90010000041b8010000004531c9488bb5a8feffff4889dfe8233b1800c744",9216,8192},
    {0x5b6444,"ba00020000b90002000041b8010000004531c9488bb5b0feffff4889dfe8ca3a1800c744",2304,1024},
    {0x5b649d,"ba00020000b90002000041b8010000004531c9488bb5b8feffff4889dfe8713a1800c744",2304,1024}};
  for(const auto& s:sites)Expect(boot,image+s.va,Hex(s.sig));
  constexpr float native[4][4]={{0,0,.5f,.5f},{.5f,0,.5f,.5f},{0,.5f,.5f,.5f},{.5f,.5f,.5f,.5f}};
  constexpr float high[4][4]={{0,0,1.f/9,.25f},{0,.25f,1.f/9,.25f},{0,.5f,1.f/9,.25f},{1.f/9,0,8.f/9,1}};
  for(int i=0;i<4;++i) {
    Bytes expected(reinterpret_cast<const unsigned char*>(native[i]),reinterpret_cast<const unsigned char*>(native[i])+16);
    Expect(boot,image+0x12f38e0+i*16,expected);
  }
  for(const auto& s:sites){Set(boot,image+s.va+1,s.w);Set(boot,image+s.va+6,s.h);}
  for(int i=0;i<4;++i)std::memcpy(boot.data()+image+0x12f38e0+i*16,high[i],16);
}

// Apply only after existing budget/rear guards have accepted the old packed atlas.
// All ten sanctioned edits are checked before committing the complete boot image.
void PatchUniform2xHighBoot(Bytes& boot) {
  if(!MatchesBootHash(boot,"5b6b564a0ef22c1bca789a6add1fa2a47a7c1b63c354417815134597b0565734","a2f9463f2ebd3fc9c04e8b8a87a9ebfae4c0ce22c28ffde21b47c2ec18e3b943"))
    Fail("High sun shadows require the supported prepared game image.");
  Expect(boot,0,Text("BBPROBE6"));const auto size=Get<uint64_t>(boot,8);
  if(size>boot.size()||boot.size()-size!=0x348180)Fail("Unsupported High sun shadow image layout.");
  const auto prefix=boot.size()-size;
  struct Edit {size_t address;const char* before;const char* after;};
  const Edit edits[]={
    {0x5b63ec,"00240000","00250000"},
    {0x5b63f1,"00200000","002a0000"},
    {0x5b6445,"00090000","40090000"},
    {0x5b644a,"00040000","40050000"},
    {0x5b649e,"00090000","40090000"},
    {0x5b64a3,"00040000","40050000"},
    {0x12f38e0,"0000000000000000398ee33d0000803e","1c4c113f4992243fc9675d3d310cc33d"},
    {0x12f38f0,"000000000000803e398ee33d0000803e","1c4c113f310cc33e5a37183e6218863e"},
    {0x12f3900,"000000000000003f398ee33d0000803e","00000000000000001c4c113f0000803f"},
    {0x12f3910,"398ee33d00000000398e633f0000803f","1c4c113f00000000c967dd3e310cc33e"},
  };
  for(const auto& e:edits)Expect(boot,prefix+e.address,Hex(e.before));
  auto candidate=boot;
  for(const auto& e:edits){auto bytes=Hex(e.after);std::copy(bytes.begin(),bytes.end(),candidate.begin()+prefix+e.address);}
  if(!MatchesBootHash(candidate,"afbcf9a092038ac6bded8a8e905a87de8dfa20dda6f4edc90d95cca525b8f0a0","6dc230d449e539771d813aa6fc8e7172dc2a398727679d5b7f7daa8fddfd3dfd"))
    Fail("High sun shadow preparation failed verification.");
  boot.swap(candidate);
}

// Intermediate High is a native, unproxied packing. Validate every sanctioned
// allocation/rectangle site before committing. Standard and Ultra stay unchanged.
void PatchIntermediateHighBoot(Bytes& boot) {
  if(!MatchesBootHash(boot,"5b6b564a0ef22c1bca789a6add1fa2a47a7c1b63c354417815134597b0565734","a2f9463f2ebd3fc9c04e8b8a87a9ebfae4c0ce22c28ffde21b47c2ec18e3b943"))
    Fail("Intermediate High sun shadows require the supported prepared game image.");
  Expect(boot,0,Text("BBPROBE6"));const auto size=Get<uint64_t>(boot,8);
  if(size>boot.size()||boot.size()-size!=0x348180)Fail("Unsupported intermediate High image layout.");
  const auto prefix=boot.size()-size;
  struct Edit {size_t address;const char* before;const char* after;};
  const Edit edits[]={
    {0x5b63ec,"00240000","00380000"},
    {0x5b63f1,"00200000","00200000"},
    {0x5b6445,"00090000","000e0000"},
    {0x5b644a,"00040000","00040000"},
    {0x5b649e,"00090000","000e0000"},
    {0x5b64a3,"00040000","00040000"},
    {0x12f38e0,"0000000000000000398ee33d0000803e","2549923e0000003f2549923d0000803e"},
    {0x12f38f0,"000000000000803e398ee33d0000803e","2549923e000000002549123e0000003f"},
    {0x12f3900,"000000000000003f398ee33d0000803e","00000000000000002549923e0000803f"},
    {0x12f3910,"398ee33d00000000398e633f0000803f","b76ddb3e000000002549123f0000803f"},
  };
  for(const auto& e:edits)Expect(boot,prefix+e.address,Hex(e.before));
  auto candidate=boot;
  for(const auto& e:edits){auto bytes=Hex(e.after);std::copy(bytes.begin(),bytes.end(),candidate.begin()+prefix+e.address);}
  if(!MatchesBootHash(candidate,"1d85d86cd627e407992ef21780a65ac570bbe87a04b93721b81a1cfb82318e64","a4baf7a7e8f2fbf2cecf18c775b161d45e394b71783eacc76c7c757878dfa3a6"))Fail("Intermediate High sun shadow preparation failed verification.");
  boot.swap(candidate);
}

struct Record {uint64_t address;Bytes data;};
void AppendRearNativeRecords(Bytes& patches,const Bytes& boot,const RearNativePayload& payload=kRearNativePayload) {
  // This gate stays closed until the independently audited binary is embedded.
  if(!payload.approved||!payload.data||!payload.size)
    Fail("The approved rear-view shadow allocation payload is unavailable.");
  if(!MatchesBootHash(boot,"5b6b564a0ef22c1bca789a6add1fa2a47a7c1b63c354417815134597b0565734","a2f9463f2ebd3fc9c04e8b8a87a9ebfae4c0ce22c28ffde21b47c2ec18e3b943"))
    Fail("Unsupported atlas image for the rear-view allocation patch.");
  Bytes stub(payload.data,payload.data+payload.size);
  if(Hash(stub)!=payload.sha256)Fail("The rear-view allocation payload failed verification.");
  const uint64_t hook=0x5b92d6,cave=0x1551b00,rxSize=0x1551ab8;
  if(payload.size>0x1552000-cave)Fail("The rear-view payload exceeds its executable padding.");
  Expect(boot,0,Text("BBPROBE6"));const auto imageSize=Get<uint64_t>(boot,8);
  if(imageSize>boot.size())Fail("Invalid image size.");const auto prefix=boot.size()-imageSize;
  if(prefix!=0x348180)Fail("Unsupported prepared image metadata.");
  const auto ns=Get<uint64_t>(boot,24),nr=Get<uint64_t>(boot,32),imports=Get<uint64_t>(boot,40);
  if(ns!=18||nr!=103126)Fail("Unsupported image segment/relocation metadata.");
  size_t at=96;const auto nm=Get<uint64_t>(boot,at);at+=8+nm*96;
  const auto nb=Get<uint64_t>(boot,at);at+=8+nb*24;
  if(Get<uint64_t>(boot,at)!=0||Get<uint64_t>(boot,at+8)!=rxSize||Get<uint64_t>(boot,at+16)!=5)
    Fail("The rear-view code padding is not in the expected executable segment.");
  if(cave<rxSize||cave+payload.size>((rxSize+4095)&~uint64_t(4095)))
    Fail("The rear-view code is outside the mapped executable padding.");
  const auto reloc=at+ns*24+imports*128;
  if(reloc+nr*32!=prefix)Fail("Unsupported relocation table layout.");
  auto overlaps=[](uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m&&b<a+n;};
  for(uint64_t i=0;i<nr;++i){const auto a=Get<uint64_t>(boot,reloc+i*32);
    if(overlaps(a,8,hook,5)||overlaps(a,8,cave,payload.size))Fail("Image relocations overlap the rear-view patch.");
  }
  Expect(boot,prefix+hook,Hex("e8954b0300"));
  Expect(boot,prefix+cave,Bytes(payload.size,0));
  Expect(patches,0,Text("BBPATCH2"));if(Get<uint64_t>(patches,8)!=0x400000)Fail("Unsupported native patch image base.");
  const auto count=Get<uint64_t>(patches,16);if(count>10000)Fail("Invalid native patch count.");
  size_t p=24;
  for(uint64_t i=0;i<count;++i) {
    auto a=Get<uint64_t>(patches,p),n=Get<uint64_t>(patches,p+8);p+=16;
    if(n>patches.size()-p)Fail("Truncated native patch record.");
    if(overlaps(a,n,hook,5)||overlaps(a,n,cave,payload.size))
      Fail("Existing native patches overlap the rear-view allocation patch.");
    p+=n;
  }
  if(p!=patches.size())Fail("Invalid native patch record end.");
  Bytes branch{0xe9};Append(branch,int32_t(cave-(hook+5)));
  // Commit only after all guards. Existing records/order/bytes are untouched.
  Bytes result=patches;Set(result,16,count+2);
  Append(result,hook);Append(result,uint64_t(branch.size()));result.insert(result.end(),branch.begin(),branch.end());
  Append(result,cave);Append(result,uint64_t(stub.size()));result.insert(result.end(),stub.begin(),stub.end());
  patches.swap(result);
}
void PatchBudget(Bytes& patches) {
  Expect(patches,0,Text("BBPATCH2"));if(Get<uint64_t>(patches,8)!=0x400000)Fail("Unsupported native patch image base.");
  const auto n=Get<uint64_t>(patches,16);if(n>10000)Fail("Invalid native patch count.");
  size_t pos=24;std::vector<Record> records;
  for(uint64_t i=0;i<n;++i) {
    auto a=Get<uint64_t>(patches,pos),len=Get<uint64_t>(patches,pos+8);pos+=16;
    if(len>patches.size()-pos)Fail("Truncated native patch entry.");
    Bytes data(patches.begin()+pos,patches.begin()+pos+len);pos+=len;
    records.push_back({a,std::move(data)});
  }
  if(pos!=patches.size())Fail("Invalid native memory patch layout.");
  struct Profile {uint64_t wrapper,arena;const char* hook;const char* arenaHook;const char* arenaCode;const char* wrapperCode;};
  // Exact canonical Memory for up to1440p/4K/8K metadata. Higher profiles must
  // retain their existing3/8GiB rendering and1/2GiB shared overrides, not be
  // reduced to the tier1 name-scoped768MiB experiment.
  const Profile profiles[]={
    {0x1257f00,0x1257f40,"e9dbf4ce00","e934d0bd00","48b80000008001000000e9c12f42ff",
     "4883ec08e8270b31ff4883c408483d00008020750c48b80000008000000000eb12483d0000000b750a48b80000002000000000c3"},
    {0x1257f60,0x1257fa0,"e93bf5ce00","e994d0bd00","48b8000000c001000000e9612f42ff",
     "4883ec08e8c70a31ff4883c408483d00008020750c48b8000000c000000000eb12483d0000000b750a48b80000004000000000c3"},
    {0x1257fc0,0x1258000,"e99bf5ce00","e9f4d0bd00","48b80000000003000000e9012f42ff",
     "4883ec08e8670a31ff4883c408483d00008020750c48b80000000002000000eb12483d0000000b750a48b80000008000000000c3"}};
  auto exact=[&](uint64_t a,const char* hex) {
    int count=0;bool match=false;for(const auto& rec:records)if(rec.address==a){++count;match=rec.data==Hex(hex);}return count==1&&match;
  };
  int tier=-1;
  for(int i=0;i<3;++i){const auto& p=profiles[i];
    if(exact(0x568a20,p.hook)&&exact(0x67af07,p.arenaHook)&&exact(p.arena,p.arenaCode)&&exact(p.wrapper,p.wrapperCode))tier=i;
  }
  if(tier<0)Fail("Sun shadows require a supported canonical memory profile. Select Standard or prepare the supported build again.");
  const auto& profile=profiles[tier];
  const std::pair<uint64_t,size_t> critical[]={{0x568a20,5},{0x67af07,5},{profile.wrapper,Hex(profile.wrapperCode).size()},{profile.arena,Hex(profile.arenaCode).size()}};
  for(const auto& rec:records)for(const auto& [a,len]:critical)
    if(rec.address!=a && rec.address<a+len && rec.address+rec.data.size()>a)
      Fail("Native patches overlap a canonical memory profile.");
  if(tier>0)return; // Canonical4K/8K patches are already larger; preserve every byte.
  int wrapper=-1;
  for(size_t i=0;i<records.size();++i){const auto& rec=records[i];
    if(rec.address==0x1257f00)wrapper=int(i);
    else if(rec.address<0x1257f40&&rec.address+rec.data.size()>0x1257f00)
      Fail("Native patches overlap the sun shadow budget wrapper.");
  }
  auto code=Hex("56e82a0b31ff59483d000080207506b800000080c3483d0000000b7517488d15466a05004839d17506b800000030c3b800000020c3");
  // Relative targets are validated by the CPU test and are stable under image relocation.
  if(0x1257f00+6+Get<int32_t>(code,2)!=0x568a30)Fail("Invalid budget CALL relocation.");
  Expect(code,29,Hex("488d15"));if(0x1257f00+36+Get<int32_t>(code,32)!=0x12ae96a)Fail("Invalid budget name relocation.");
  code.resize(64,0x90);records[wrapper].data=std::move(code);
  Bytes result(patches.begin(),patches.begin()+24);
  for(const auto& rec:records){Append(result,rec.address);Append(result,uint64_t(rec.data.size()));result.insert(result.end(),rec.data.begin(),rec.data.end());}
  patches.swap(result);
}
std::string ArchiveStamp(const fs::path& p) {
  std::error_code ec;const auto size=fs::file_size(p,ec);if(ec)return "missing";
  const auto time=fs::last_write_time(p,ec);if(ec)return "missing";
  return std::to_string(size)+":"+std::to_string((long long)time.time_since_epoch().count());
}
std::string ChoiceStamp(const GraphicsChoice& c) {
  std::ostringstream o;o<<c.own_car<<','<<c.opponents<<','<<c.scenery<<','<<c.reflections<<','<<c.shadows<<','
    <<c.particles<<','<<c.dynamic_res<<','<<c.motion_blur<<','<<c.lens_dust<<','<<c.heat_haze<<','<<c.fxaa<<','<<c.sun_shadows;
  return o.str();
}
}

std::wstring PrepareSunShadows(const std::wstring& prep,const std::wstring& cache,
                              const std::wstring& update,const std::wstring& base,
                              const std::wstring& runtime,const GraphicsChoice& requested_choice,
                              SunShadowBundle& bundle) {
  auto choice=requested_choice;choice.sun_shadows=GraphicsChoice::SunShadowSelection(choice.sun_shadows);
  bundle={};fs::path stage;
  try {
    if(choice.sun_shadows<1||choice.sun_shadows>2)Fail("Invalid sun shadow profile.");
    if(!SupportsRuntime(runtime))Fail("This runtime does not support the selected sun shadow atlas. Update the runtime or select Standard.");
    if(Hash(Read(runtime))!=kApprovedNativeRuntimeSha256)
      Fail("Sun shadows require the matching updated runtime.");
    auto boot=Read(fs::path(prep)/L"boot-linked.bin"),patches=Read(fs::path(prep)/L"patches.bin");
    const auto sourceStamp=std::string(choice.sun_shadows==2?kHighVersion:kSunVersion)+"\n"+Hash(boot)+"\n"+Hash(patches)+"\n"+
        ArchiveStamp(fs::path(update)/L"game.ndx")+"\n"+ArchiveStamp(fs::path(base)/L"game.ndx")+"\n"+ChoiceStamp(choice);
    PatchBoot(boot);PatchBudget(patches);AppendRearNativeRecords(patches,boot);
    if(choice.sun_shadows==2)PatchIntermediateHighBoot(boot);
    const auto target=fs::path(cache)/Hash(Text(sourceStamp));
    const auto expectedPrefix=sourceStamp+"\n"+Hash(boot)+"\n"+Hash(patches)+"\n";
    bool cached=false;
    if(fs::exists(target/L"ready.txt"))try {
      const auto pack=Read(target/L"global.rpk");
      cached=Read(target/L"ready.txt")==Text(expectedPrefix+Hash(pack)) && Read(target/L"boot-linked.bin")==boot && Read(target/L"patches.bin")==patches;
    } catch(...) {cached=false;}
    if(!cached) {
      fs::create_directories(cache);
      stage=fs::path(cache)/(L".stage-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
      fs::create_directory(stage);Write(stage/L"boot-linked.bin",boot);Write(stage/L"patches.bin",patches);
      bool active=false;auto error=BuildGraphics(update,base,stage.wstring(),choice,active);
      if(!error.empty()||!active)throw std::runtime_error("The complete sun shadow graphics pack could not be prepared.");
      const auto pack=Read(stage/L"global.rpk");Write(stage/L"ready.txt",Text(expectedPrefix+Hash(pack)));
      // Directory rename publishes all files as one generation. A failed generation
      // cannot be selected, and the previously prepared bundle remains intact.
      fs::path old;
      if(fs::exists(target)){old=target.wstring()+L".old-"+std::to_wstring(GetTickCount64());fs::rename(target,old);}
      try {fs::rename(stage,target);stage.clear();}
      catch(...) {if(!old.empty())fs::rename(old,target);throw;}
      if(!old.empty()){std::error_code ignored;fs::remove_all(old,ignored);}
    }
    bundle={ (target/L"boot-linked.bin").wstring(),(target/L"patches.bin").wstring(),target.wstring(),0 };
    return L"";
  } catch(const std::exception& e) {
    if(!stage.empty()){std::error_code ignored;fs::remove_all(stage,ignored);}
    const std::string message=e.what();return std::wstring(message.begin(),message.end());
  }
}
