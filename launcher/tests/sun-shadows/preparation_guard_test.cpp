// Synthetic fixtures only: runtime capability and refusal must precede game I/O.
#include "../../sun_shadows.cpp"
#include <cassert>
#include <iostream>
std::wstring BuildGraphics(const std::wstring&,const std::wstring&,const std::wstring&,const GraphicsChoice&,bool&) {
  throw std::runtime_error("Unexpected graphics preparation");
}
int main(int argc,char** argv) {
  assert(argc==2);const fs::path root=argv[1];fs::create_directories(root);
  const auto runtime=root/"runtime.bin";
  Write(runtime,Text("BB_SUN_SHADOWS_LIMITED_V1"));assert(!SupportsRuntime(runtime));
  GraphicsChoice choice;choice.sun_shadows=3;SunShadowBundle bundle{L"sentinel",L"sentinel",L"sentinel",2};
  const auto error=PrepareSunShadows((root/"absent").wstring(),(root/"cache").wstring(),L"",L"",runtime.wstring(),choice,bundle);
  assert(error.find(L"does not support")!=std::wstring::npos);
  assert(bundle.boot.empty()&&bundle.patches.empty()&&bundle.graphics.empty()&&bundle.proxy_scale==0);
  assert(!fs::exists(root/"cache"));
  std::string split(65530,'x');split+="BB_SUN_SHADOWS_FULL_V1";Write(runtime,Text(split));assert(SupportsRuntime(runtime));
  choice.sun_shadows=4;assert(PrepareSunShadows(L"",L"",L"",L"",runtime.wstring(),choice,bundle)==L"Invalid sun shadow profile.");
  Bytes truncated;bool refused=false;try{PatchBoot(truncated);}catch(const std::exception&){refused=true;}assert(refused);
  std::cout<<"PASS capability boundary, unsupported runtime refusal, empty bundle, no cache publication, invalid profile, truncated game refusal\n";
}
