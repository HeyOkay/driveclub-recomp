// Included inside namespace BbOverlay. All calls run under imgui_mutex;
// this panel must never call input/pad sampling or functions that reacquire it.
namespace {
void SunShadowPanel() {
    using namespace BbSunControls;
    bool open = sun_panel_open.load();
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 24 * base_scale,
                                 viewport->WorkPos.y + 24 * base_scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(510 * base_scale, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(420 * base_scale, 0),
                                      ImVec2(viewport->WorkSize.x * .9f, viewport->WorkSize.y * .9f));
    const bool expanded = ImGui::Begin("Sun shadows  |  F11", &open, ImGuiWindowFlags_AlwaysAutoResize);
    if (!open) {
        sun_panel_open=false;
        runtime_exit_release_keys();
    }
    if (!expanded) { ImGui::End(); return; }
    ImGui::TextDisabled("F11 or Escape closes. Filter changes apply live.");
    static std::string save_status;
    const bool live_available = LiveAvailable();
    if (!live_available) ImGui::TextWrapped("Enable enhanced maps and restart to use live PCSS controls.");
    ImGui::BeginDisabled(!live_available);
    auto params = Snapshot();
    bool changed = false;
    bool pcss = live_available && params.mode == 2;
    if (ImGui::Checkbox("PCSS contact-hardening shadows", &pcss)) {
        params.mode = pcss ? 2 : 0;
        changed = true;
    }
    if (!live_available) ImGui::TextWrapped("This session keeps native filtering and the original visibility response.");
    else if (params.mode == 0) ImGui::TextWrapped("Native filtering and original visibility response.");
    else if (params.mode == 1) ImGui::TextWrapped("Diagnostic: native PCF kernel with linear visibility; PCSS widening is off.");
    else ImGui::TextWrapped("Softness grows with the distance between caster and receiver.");

    if (ImGui::Button("Original filtering")) { params.mode=0; changed=true; }
    ImGui::SameLine();
    if (ImGui::Button("PCSS defaults")) { Reset(); params=Snapshot(); changed=false; save_status.clear(); }
    ImGui::Separator();
    ImGui::BeginDisabled(params.mode != 2);
    changed |= ImGui::SliderFloat("Sun radius (degrees)", &params.sun_radius_deg, .05f, 2.0f, "%.2f");
    changed |= ImGui::SliderFloat("Minimum footprint", &params.minimum_footprint_scale, .25f, 2.0f, "%.2fx");
    int pattern = int(params.pattern);
    if (ImGui::Combo("Sample pattern", &pattern, "Native\0Disk\0")) {
        params.pattern = unsigned(pattern);
        if (pattern == 0) {
            params.filter_samples = std::min(params.filter_samples, 32u);
            params.blocker_samples = std::min(params.blocker_samples, 32u);
        }
        changed = true;
    }
    const int cap = params.pattern == 0 ? 32 : 128;
    int filter = std::min(int(params.filter_samples), cap);
    int blockers = std::min(int(params.blocker_samples), cap);
    if (ImGui::SliderInt("Filter samples", &filter, 8, cap)) { params.filter_samples=unsigned(filter); changed=true; }
    if (ImGui::SliderInt("Blocker samples", &blockers, 8, cap)) { params.blocker_samples=unsigned(blockers); changed=true; }
    if (params.pattern == 0) {
        ImGui::TextWrapped("Native pattern: at most 32 samples per stage; the native per-draw count can be lower.");
        if (params.filter_samples > 32 || params.blocker_samples > 32)
            ImGui::TextDisabled("Stored request is capped at 32 here. Choose Disk for more samples.");
    } else ImGui::TextDisabled("Disk pattern: 8 to 128 samples per stage.");
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader("Advanced diagnostics")) {
        int mode = int(params.mode);
        if (ImGui::Combo("Filter mode", &mode, "Native original\0Linear PCF (diagnostic)\0Physical PCSS\0")) {
            params.mode=unsigned(mode); changed=true;
        }
        ImGui::BeginDisabled(params.mode != 2);
        auto flag = [&](const char* label, unsigned bit) {
            bool enabled = (params.flags & bit) != 0;
            if (ImGui::Checkbox(label, &enabled)) {
                params.flags = enabled ? params.flags | bit : params.flags & ~bit;
                changed = true;
            }
        };
        flag("Receiver-plane correction", 1);
        flag("Receiver self-rejection", 2);
        flag("Linear visibility response", 4);
        ImGui::TextWrapped("Self-rejection never enables blockers behind the receiver. Linear visibility changes partial-shadow contrast.");
        ImGui::EndDisabled();
    }
    if (changed) { Set(params); save_status.clear(); }
    if (ImGui::Button("Save live settings")) {
        std::string error;
        save_status = Save(error) ? "Live settings saved." : "Save failed: " + error;
    }
    if (!save_status.empty()) ImGui::TextWrapped("%s", save_status.c_str());
    ImGui::EndDisabled();

    ImGui::SeparatorText("Resources — next launch");
    static int next_profile = std::clamp(LoadedProfile(), 0, 2);
    const int loaded = LoadedProfile();
    const char* names[] = {"Stock maps and range", "High-resolution maps", "High-resolution maps + long range"};
    if (loaded >= 0 && loaded <= 2) ImGui::TextWrapped("Loaded: %s", names[loaded]);
    else ImGui::TextDisabled("Loaded resource profile is unavailable.");
    const bool available = ProfileAvailable();
    ImGui::BeginDisabled(!available);
    static std::string profile_status;
    bool high = next_profile != 0, long_range = next_profile == 2;
    if (ImGui::Checkbox("High-resolution maps", &high)) { next_profile = high ? 1 : 0; profile_status.clear(); }
    if (!high) long_range=false;
    ImGui::BeginDisabled(!high);
    if (ImGui::Checkbox("Long range", &long_range)) { next_profile = long_range ? 2 : 1; profile_status.clear(); }
    ImGui::EndDisabled();
    if (ImGui::Button("Save resource choice for next launch")) {
        std::string error;
        profile_status = SaveNextProfile(next_profile, error)
            ? std::string("Saved for next launch: ") + names[next_profile]
            : "Resource choice was not saved: " + error;
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Map allocation and shadow range change after restarting through the launcher.");
    if (!available) ImGui::TextWrapped("Start through the preview launcher to save the next-launch resource choice.");
    if (!profile_status.empty()) ImGui::TextWrapped("%s", profile_status.c_str());
    ImGui::End();
}
} // namespace
