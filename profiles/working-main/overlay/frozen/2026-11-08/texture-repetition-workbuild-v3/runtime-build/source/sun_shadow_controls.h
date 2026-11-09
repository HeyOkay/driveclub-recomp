#pragma once
// Shared by the overlay and the authorized sunlight pipeline only. No Vulkan
// types or renderer ABI additions; DrawParams occupies eight existing UD words.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace BbSunControls {
inline constexpr std::uint32_t Magic = 0x32534e50;
inline constexpr std::uint32_t Native = 0, LinearPcf = 1, Pcss = 2;
inline constexpr std::uint32_t ReceiverPlane = 1, SelfRejection = 2, LinearVisibility = 4;
inline constexpr std::uint32_t NativePattern = 0, DiskPattern = 1;
struct DrawParams {
    std::uint32_t magic = Magic;
    std::uint32_t mode = Pcss;
    std::uint32_t filter_samples = 32;
    std::uint32_t blocker_samples = 32;
    float sun_radius_deg = 0.27f;
    float minimum_footprint_scale = 1.0f;
    std::uint32_t flags = ReceiverPlane | SelfRejection | LinearVisibility;
    std::uint32_t pattern = NativePattern;
};
static_assert(sizeof(DrawParams) == 32 && std::is_trivially_copyable_v<DrawParams>);
static_assert(offsetof(DrawParams, mode) == 4 && offsetof(DrawParams, filter_samples) == 8 &&
    offsetof(DrawParams, blocker_samples) == 12 && offsetof(DrawParams, sun_radius_deg) == 16 &&
    offsetof(DrawParams, minimum_footprint_scale) == 20 && offsetof(DrawParams, flags) == 24 &&
    offsetof(DrawParams, pattern) == 28);

namespace Detail {
inline std::string Trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == s.npos) return {};
    return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}
inline std::filesystem::path EnvPath(const char* name) {
#ifdef _WIN32
    std::wstring wide_name;
    for (const auto* p = name; *p; ++p) wide_name += wchar_t(*p);
    const auto n = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (!n) return {};
    std::wstring value(n, L'\0');
    const auto actual = GetEnvironmentVariableW(wide_name.c_str(), value.data(), n);
    if (!actual || actual >= n) return {};
    value.resize(actual);
    return std::filesystem::path(value);
#else
    const char* value = std::getenv(name);
    return value && *value ? std::filesystem::path(value) : std::filesystem::path{};
#endif
}
inline DrawParams Clamp(DrawParams p) {
    p.magic = Magic;
    if (p.mode > Pcss) p.mode = Pcss;
    p.filter_samples = std::clamp(p.filter_samples, 8u, 128u);
    p.blocker_samples = std::clamp(p.blocker_samples, 8u, 128u);
    p.sun_radius_deg = std::isfinite(p.sun_radius_deg) ? std::clamp(p.sun_radius_deg, 0.05f, 2.0f) : 0.27f;
    p.minimum_footprint_scale = std::isfinite(p.minimum_footprint_scale) ?
        std::clamp(p.minimum_footprint_scale, 0.25f, 2.0f) : 1.0f;
    p.flags &= 7u;
    if (p.pattern > DiskPattern) p.pattern = NativePattern;
    return p;
}
inline bool Read(const std::filesystem::path& path, std::string& data, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 1024 * 1024) { error = "Settings file is missing, unreadable, or too large."; return false; }
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Cannot open settings file."; return false; }
    data.assign(std::istreambuf_iterator<char>(file), {});
    if (!file.eof() && file.fail()) { error = "Cannot read settings file."; return false; }
    return true;
}
inline bool AtomicWrite(const std::filesystem::path& path, const std::string& data, std::string& error,
                        const std::string* expected = nullptr) {
    static std::atomic<unsigned> next{0};
    const auto suffix = std::to_string(next.fetch_add(1));
#ifdef _WIN32
    auto temp = path;
    temp += L".sun-tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::wstring(suffix.begin(), suffix.end());
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = "Cannot create temporary settings file."; return false; }
    DWORD written = 0;
    const bool wrote = data.size() <= MAXDWORD && WriteFile(file, data.data(), DWORD(data.size()), &written, nullptr) &&
                       written == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!wrote) { DeleteFileW(temp.c_str()); error = "Cannot write settings file."; return false; }
#else
    auto temp = path; temp += ".sun-tmp-" + suffix;
    { std::ofstream file(temp, std::ios::binary | std::ios::trunc); file.write(data.data(), data.size());
      if (!file) { std::filesystem::remove(temp); error = "Cannot write settings file."; return false; } }
#endif
    if (expected) {
        std::string current;
        if (!Read(path, current, error) || current != *expected) {
            std::error_code ec; std::filesystem::remove(temp, ec);
            error = "Launcher settings changed while saving. Try again."; return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str()); error = "Cannot replace settings file."; return false;
    }
#else
    std::error_code ec; std::filesystem::rename(temp, path, ec);
    if (ec) { std::filesystem::remove(temp, ec); error = "Cannot replace settings file."; return false; }
#endif
    error.clear(); return true;
}
inline void LoadField(DrawParams& p, std::string_view name, const std::string& text) {
    try {
        size_t used = 0;
        if (name == "sun_radius_deg" || name == "minimum_footprint_scale") {
            const float f = std::stof(text, &used);
            if (used != text.size()) return;
            if (name == "sun_radius_deg") p.sun_radius_deg = f; else p.minimum_footprint_scale = f;
        } else {
            if (text.empty() || text[0] == '-') return;
            const auto n = std::stoull(text, &used);
            if (used != text.size() || n > UINT32_MAX) return;
            if (name == "mode") p.mode = std::uint32_t(n);
            else if (name == "filter_samples") p.filter_samples = std::uint32_t(n);
            else if (name == "blocker_samples") p.blocker_samples = std::uint32_t(n);
            else if (name == "flags") p.flags = std::uint32_t(n);
            else if (name == "pattern") p.pattern = std::uint32_t(n);
        }
    } catch (...) { /* Invalid user tuning retains its default. */ }
}
struct State {
    std::mutex writer;
    std::atomic<std::uint64_t> sequence{0};
    std::array<std::atomic<std::uint32_t>, 8> words{};
    State() {
        DrawParams p;
        const auto dir = EnvPath("BB_GPU_USER_DIR");
        if (!dir.empty()) {
            std::string data, error;
            if (Read(dir / "sun-shadow-controls.ini", data, error)) {
                std::istringstream lines(data); std::string line;
                while (std::getline(lines, line)) {
                    const auto eq = line.find('=');
                    if (eq != line.npos) LoadField(p, Trim(std::string_view(line).substr(0, eq)), Trim(std::string_view(line).substr(eq + 1)));
                }
            }
        }
        const auto bits = std::bit_cast<std::array<std::uint32_t, 8>>(Clamp(p));
        for (unsigned i = 0; i < 8; ++i) words[i].store(bits[i], std::memory_order_relaxed);
    }
};
inline State& Current() { static State value; return value; }
struct LayoutKey {
    std::uintptr_t device, layout;
    bool operator==(const LayoutKey&) const = default;
};
struct LayoutHash { size_t operator()(const LayoutKey& k) const { return std::hash<std::uintptr_t>{}(k.layout) ^ std::hash<std::uintptr_t>{}(k.device); } };
inline std::mutex layout_mutex;
inline std::unordered_set<LayoutKey, LayoutHash> layouts;
struct ProgramPair { std::atomic<std::uint64_t> vertex{0}, fragment{0}; };
inline std::array<ProgramPair, 16> programs;
inline std::atomic<unsigned> program_count{0};
} // namespace Detail

inline DrawParams Snapshot() {
    auto& state = Detail::Current();
    for (;;) {
        const auto before = state.sequence.load();
        if (before & 1u) continue;
        std::array<std::uint32_t, 8> words;
        for (unsigned i = 0; i < 8; ++i) words[i] = state.words[i].load();
        if (state.sequence.load() == before) return std::bit_cast<DrawParams>(words);
    }
}
inline void Set(const DrawParams& input) {
    auto& state = Detail::Current();
    const auto words = std::bit_cast<std::array<std::uint32_t, 8>>(Detail::Clamp(input));
    std::scoped_lock lock{state.writer};
    state.sequence.fetch_add(1);
    for (unsigned i = 0; i < 8; ++i) state.words[i].store(words[i]);
    state.sequence.fetch_add(1);
}
inline void Reset() { Set(DrawParams{}); } // Live reset; disk changes require Save.
inline bool Save(std::string& error) {
    const auto dir = Detail::EnvPath("BB_GPU_USER_DIR");
    if (dir.empty()) { error = "This launch did not provide a user settings folder."; return false; }
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) { error = "User settings folder is unavailable."; return false; }
    const auto p = Snapshot();
    std::ostringstream text;
    text << "mode=" << p.mode << "\nfilter_samples=" << p.filter_samples << "\nblocker_samples=" << p.blocker_samples
         << "\nsun_radius_deg=" << p.sun_radius_deg << "\nminimum_footprint_scale=" << p.minimum_footprint_scale
         << "\nflags=" << p.flags << "\npattern=" << p.pattern << '\n';
    return Detail::AtomicWrite(dir / "sun-shadow-controls.ini", text.str(), error);
}
inline int LoadedProfile() {
    const char* value = std::getenv("BB_SUN_SHADOW_PROFILE");
    return value && value[0] >= '0' && value[0] <= '2' && !value[1] ? value[0] - '0' : -1;
}
inline bool LiveAvailable() {
    const char* quality = std::getenv("BB_SUN_SHADOW_QUALITY");
    return LoadedProfile() > 0 && quality &&
           (std::strcmp(quality, "pcss") == 0 || std::strcmp(quality, "pcss-diagnostic") == 0);
}
inline bool ProfileAvailable() {
    const auto path = Detail::EnvPath("BB_LAUNCHER_CONFIG"); std::error_code ec;
    return LoadedProfile() >= 0 && !path.empty() && std::filesystem::is_regular_file(path, ec);
}
inline bool SaveNextProfile(int profile, std::string& error) {
    if (profile < 0 || profile > 2) { error = "Unsupported next-launch shadow profile."; return false; }
    const auto path = Detail::EnvPath("BB_LAUNCHER_CONFIG");
    if (!ProfileAvailable()) { error = "This launch did not provide launcher settings. Change resources in the launcher."; return false; }
    std::string original;
    if (!Detail::Read(path, original, error)) return false;
    std::string output; bool found = false; size_t at = 0;
    while (at < original.size()) {
        auto end = original.find('\n', at); if (end == original.npos) end = original.size();
        auto line = original.substr(at, end - at);
        size_t start = at == 0 && line.starts_with("\xef\xbb\xbf") ? 3 : 0;
        const auto eq = line.find('=', start);
        if (eq != line.npos && Detail::Trim(std::string_view(line).substr(start, eq - start)) == "gfx_sun_shadows") {
            size_t first = eq + 1; while (first < line.size() && (line[first] == ' ' || line[first] == '\t')) ++first;
            size_t last = first; while (last < line.size() && line[last] != ' ' && line[last] != '\t' && line[last] != '\r' && line[last] != ';' && line[last] != '#') ++last;
            line.replace(first, last - first, std::to_string(profile)); found = true;
        }
        output += line;
        if (end < original.size()) output += '\n';
        at = end + 1;
    }
    if (!found) {
        const auto newline = original.find("\r\n") != original.npos ? "\r\n" : "\n";
        if (!output.empty() && output.back() != '\n') output += newline;
        output += "gfx_sun_shadows=" + std::to_string(profile) + newline;
    }
    return Detail::AtomicWrite(path, output, error, &original);
}

// Register only after the complete authorized replacement pair has produced a
// successful VkPipeline. The common path's hash check is merely a lock-free
// prefilter; full device/layout membership remains authoritative.
inline void RegisterLayout(std::uintptr_t device, std::uintptr_t layout, std::uint64_t vertex, std::uint64_t fragment) {
    if (!device || !layout || !vertex || !fragment) return;
    std::scoped_lock lock{Detail::layout_mutex};
    bool recorded = false;
    unsigned index = 0;
    for (auto& pair : Detail::programs) {
        const auto f = pair.fragment.load(std::memory_order_relaxed);
        if (f == fragment && pair.vertex.load(std::memory_order_relaxed) == vertex) { recorded = true; break; }
        if (!f) { pair.vertex.store(vertex, std::memory_order_relaxed); pair.fragment.store(fragment, std::memory_order_release); Detail::program_count.store(index + 1, std::memory_order_release); recorded = true; break; }
        ++index;
    }
    if (recorded) Detail::layouts.insert({device, layout}); // Full slots fail closed.
}
inline bool LikelySun(std::uint64_t vertex, std::uint64_t fragment) {
    const unsigned count = Detail::program_count.load(std::memory_order_acquire);
    for (unsigned index = 0; index < count; ++index) {
        const auto& pair = Detail::programs[index];
        const auto f = pair.fragment.load(std::memory_order_acquire);
        if (f && f == fragment && pair.vertex.load(std::memory_order_relaxed) == vertex) return true;
    }
    return false;
}
inline bool Authorized(std::uintptr_t device, std::uintptr_t layout) {
    std::scoped_lock lock{Detail::layout_mutex}; return Detail::layouts.contains({device, layout});
}
inline void UnregisterLayout(std::uintptr_t device, std::uintptr_t layout) {
    std::scoped_lock lock{Detail::layout_mutex}; Detail::layouts.erase({device, layout});
}
} // namespace BbSunControls
