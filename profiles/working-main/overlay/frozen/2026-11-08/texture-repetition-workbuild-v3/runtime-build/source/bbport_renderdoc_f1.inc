// Local capture-only adapter. SDK header is RenderDoc's official MIT application API.
#include "renderdoc_app.h"
#include "capture_trigger_file.h"
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
RENDERDOC_API_1_6_0* bb_capture_api{};
std::atomic<bool> bb_capture_ready{false};
BbCaptureTrigger::Request bb_file_request;
uint32_t bb_capture_frames=2;
}
namespace BbOverlay::SceneDebug { extern std::atomic<bool> expanded; }
namespace VideoCore {
void LoadRenderDoc() {
    const char* frames=std::getenv("BB_RENDERDOC_CAPTURE_FRAMES");
    bb_capture_frames=frames && frames[0]=='1' && frames[1]=='\0' ? 1u : 2u;
    BbOverlay::SceneDebug::expanded.store(true,std::memory_order_relaxed);
    std::puts("RenderDocF1: expanded statistics enabled for isolated capture build");
#ifdef _WIN32
    const wchar_t* path = _wgetenv(L"BB_RENDERDOC_DLL");
    if (!path || !*path) return;
    HMODULE module = LoadLibraryW(path);
    auto get_api = module ? reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module,"RENDERDOC_GetAPI")) : nullptr;
    if (!get_api || get_api(eRENDERDOC_API_Version_1_6_0,reinterpret_cast<void**>(&bb_capture_api)) != 1) {
        std::printf("RenderDocF1: unavailable (Win32 %lu)\n", GetLastError()); std::fflush(stdout); return;
    }
    RENDERDOC_InputButton key = eRENDERDOC_Key_F1;
    bb_capture_api->SetCaptureKeys(&key,1);
    bb_capture_api->SetFocusToggleKeys(nullptr,0);
    bb_capture_api->UnloadCrashHandler();
    bb_capture_api->MaskOverlayBits(0,0);
    bb_capture_api->SetCaptureOptionU32(eRENDERDOC_Option_RefAllResources,0);
    bb_capture_api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks,0);
    bb_capture_api->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation,0);
    bb_capture_api->SetCaptureOptionU32(eRENDERDOC_Option_SoftMemoryLimit,1024);
    if (const char* destination=std::getenv("BB_FULL_CAPTURE_PATH"); destination && *destination)
        bb_capture_api->SetCaptureFilePathTemplate(destination);
    bb_capture_ready.store(true,std::memory_order_release);
    bb_file_request.ArmFromEnvironment();
    std::puts("RenderDocF1: API1.6.0 ready before Vulkan; native RenderDoc F1 captures one frame, F12 remains game statistics");
    std::fflush(stdout);
#endif
}
bool IsRenderDocLoaded() { return bb_capture_ready.load(std::memory_order_acquire); }
// Liverpool calls this at each command-work boundary. The API schedules the next
// two real presents; it does NOT start/end capture at guest submission boundaries.
void StartCapture() {
    if (!IsRenderDocLoaded()) return;
    bb_file_request.Poll([] { return bb_capture_api->IsFrameCapturing()!=0; },[](const char* nonce) {
        SYSTEMTIME utc{};GetSystemTime(&utc);
        const char* output=bb_capture_api->GetCaptureFilePathTemplate();
        std::printf("RenderDocFile: accepted nonce=%s utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ tick_ms=%llu tid=%lu request=%s output=%s frames=%u\n",
            nonce,utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,
            (unsigned long long)GetTickCount64(),GetCurrentThreadId(),bb_file_request.path.string().c_str(),output?output:"",bb_capture_frames);
        std::fflush(stdout);
        bb_capture_api->TriggerMultiFrameCapture(bb_capture_frames);
        std::printf("RenderDocFile: scheduled nonce=%s frames=%u tick_ms=%llu\n",nonce,bb_capture_frames,(unsigned long long)GetTickCount64());
        std::fflush(stdout);
    });
}
void EndCapture() {}
void TriggerCapture() {
    if (IsRenderDocLoaded() && !bb_capture_api->IsFrameCapturing()) bb_capture_api->TriggerCapture();
}
void SetOutputDir(const std::filesystem::path& dir,const std::string& prefix) {
    if (IsRenderDocLoaded()) {
        const char* explicit_path=std::getenv("BB_FULL_CAPTURE_PATH");
        if (!explicit_path || !*explicit_path) bb_capture_api->SetCaptureFilePathTemplate((dir/prefix).string().c_str());
    }
}
}
