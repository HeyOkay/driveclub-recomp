#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t file[32768];
    DWORD size = GetModuleFileNameW(nullptr, file, 32768);
    if (!size || size >= 32768) return 1;
    std::wstring root(file, size);
    root.resize(root.find_last_of(L"\\/"));
    const std::wstring script = root + L"\\Install-Launcher.ps1";
    if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(nullptr, L"Extract the complete launcher package before running Setup.exe.",
                    L"DRIVECLUB Launcher Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    wchar_t system[32768];
    if (!GetSystemDirectoryW(system, 32768)) return 1;
    const std::wstring powershell = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    bool check = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    for (int i = 1; i < argc; ++i) if (std::wstring(argv[i]) == L"--check-only") check = true;
    LocalFree(argv);
    std::wstring args = L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + script + L"\"";
    if (check) args += L" -CheckOnly";
    SHELLEXECUTEINFOW start = {sizeof(start)};
    start.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    start.lpFile = powershell.c_str();
    start.lpParameters = args.c_str();
    start.lpDirectory = root.c_str();
    start.nShow = SW_HIDE;
    if (!ShellExecuteExW(&start) || !start.hProcess) {
        MessageBoxW(nullptr, L"Windows PowerShell could not be started.", L"DRIVECLUB Launcher Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    WaitForSingleObject(start.hProcess, INFINITE);
    DWORD result = 1;
    GetExitCodeProcess(start.hProcess, &result);
    CloseHandle(start.hProcess);
    return static_cast<int>(result);
}
