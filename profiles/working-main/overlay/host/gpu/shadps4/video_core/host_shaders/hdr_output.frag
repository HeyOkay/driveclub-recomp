// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: HDR10 output for a game that makes an SDR picture (HostPasses::HdrPass). The finished
// frame (sRGB encoded) is shown at paper white. Where it equals the game's picture before its UI
// (the 3D scene), the highlights go towards the display's peak: from the game's linear scene where
// the SDR picture runs out (clipped skies, the sun, lights, reflections), matched to the SDR
// picture's brightness (hdr_exposure.comp), and by expanding the SDR picture's own brightest tones.
// The UI stays at paper white. Out: Rec.2020 primaries, PQ (SMPTE ST 2084) encoded.
#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D frame_tex;    // the frame, sRGB encoded values (UNORM)
layout(binding = 1) uniform sampler2D hudless_tex;  // the picture before the UI, the same way
layout(binding = 2) uniform sampler2D scene_tex;    // the linear scene (or the frame: unused)
layout(binding = 3, r32f) uniform readonly image2D state;  // SDR white per scene unit

layout(push_constant) uniform Params {
    float paper_nits;   // SDR white (UI, menus)
    float peak_nits;    // the brightest highlight
    float has_hudless;  // 0: no 3D scene in this frame (menus): everything at paper white
    float knee;         // SDR luminance where the highlights start
    float has_scene;    // the linear scene is there
};

const vec3 kLuma = vec3(0.2126, 0.7152, 0.0722);

vec3 ToLinear(vec3 c) {
    return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, lessThan(c, vec3(0.04045)));
}

vec3 Pq(vec3 nits) {
    const float m1 = 0.1593017578125, m2 = 78.84375;
    const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const vec3 y = pow(clamp(nits / 10000.0, 0.0, 1.0), vec3(m1));
    return pow((c1 + c2 * y) / (1.0 + c3 * y), vec3(m2));
}

// Rec.709 to Rec.2020 primaries (columns).
const mat3 kBt709ToBt2020 = mat3(0.6274040, 0.0690970, 0.0163916,
                                 0.3292820, 0.9195400, 0.0880132,
                                 0.0433136, 0.0113612, 0.8955950);

void main() {
    const vec3 sdr = texture(frame_tex, uv).rgb;
    const vec3 lin = ToLinear(sdr);
    // The UI: where the frame differs from the picture before it.
    float ui = 1.0;
    if (has_hudless > 0.5) {
        const vec3 d = abs(sdr - texture(hudless_tex, uv).rgb);
        ui = smoothstep(0.02, 0.06, max(d.r, max(d.g, d.b)));
    }
    // In units of SDR white. The SDR picture's brightest tones a little brighter (up to 1.5x at
    // white): white surfaces (snow, painted lines) are not lights; the light comes from the scene.
    const float range = peak_nits / paper_nits;
    const float y = dot(lin, kLuma);
    const float t = clamp((y - knee) / max(1.0 - knee, 1e-3), 0.0, 1.0);
    vec3 hdr = lin * (1.0 + (min(range, 1.5) - 1.0) * pow(t, 1.6));
    const float k = imageLoad(state, ivec2(0)).r;
    if (has_scene > 0.5 && k > 0.0) {
        // The scene at the SDR picture's brightness, rolled off smoothly towards the peak.
        const vec3 scene = max(texture(scene_tex, uv).rgb, vec3(0.0)) * k;
        const float ys = dot(scene, kLuma);
        const float rolled = ys <= 1.0 ? ys : 1.0 + (range - 1.0) * (1.0 - exp(-(ys - 1.0) / (range - 1.0)));
        // Only where the SDR picture runs out; its colour, or the scene's where it is clipped.
        const float w = smoothstep(knee, 1.0, y);
        const float y_out = max(dot(hdr, kLuma), w * rolled);
        const float clipped = smoothstep(0.85, 1.0, max(sdr.r, max(sdr.g, sdr.b))) * w;
        const vec3 hue = mix(lin / max(y, 1e-4), scene / max(ys, 1e-4), clipped);
        hdr = hue * y_out;
    }
    const vec3 nits = paper_nits * mix(hdr, lin, ui);
    color = vec4(Pq(kBt709ToBt2020 * max(nits, vec3(0.0))), 1.0);
}
