#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

// Private one-shot diagnostic request. No worker, directory discovery, file writes, or input.
namespace BbCaptureTrigger {
struct Request {
    std::filesystem::path path;
    bool armed=false, consumed=false;
    std::chrono::steady_clock::time_point next{};
    unsigned long long polls=0;
    void ArmFromEnvironment() {
        const wchar_t* value=_wgetenv(L"BB_RENDERDOC_TRIGGER_FILE");
        if (!value || !*value) return;
        path=value;
        if (!path.is_absolute()) {
            std::puts("RenderDocFile: rejected relative BB_RENDERDOC_TRIGGER_FILE");
            path.clear();return;
        }
        armed=true;
        std::printf("RenderDocFile: armed one-shot frames=2 request=%s\n",path.string().c_str());
        std::fflush(stdout);
    }
    template<class Busy, class Trigger>
    void Poll(Busy busy,Trigger trigger) {
        if (!armed || consumed) return;
        const auto now=std::chrono::steady_clock::now();
        if (now<next) return;
        next=now+std::chrono::milliseconds(100);
        ++polls;
        FILE* file=_wfopen(path.c_str(),L"rb");
        if (!file) return;
        char data[67]{};
        const auto count=std::fread(data,1,sizeof(data),file);
        const bool good=!std::ferror(file);
        std::fclose(file);
        if (!good || count==sizeof(data)) return;
        size_t size=count;
        while (size && (data[size-1]=='\r' || data[size-1]=='\n')) --size;
        if (!size || size>64) return;
        for (size_t i=0;i<size;++i) {
            const char c=data[i];
            if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return;
        }
        data[size]=0;
        if (busy()) return; // Existing capture finishes first; do not consume its request.
        consumed=true; // Set before API call: never repeat even if API blocks/re-enters.
        trigger(data);
    }
};
}
