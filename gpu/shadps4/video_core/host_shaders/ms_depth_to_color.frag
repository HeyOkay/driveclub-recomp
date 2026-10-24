// SPDX-FileCopyrightText: Copyright 2026 DriveClub Recomp contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450 core
#extension GL_EXT_samplerless_texture_functions : require

// Reads a multi-sample depth image as the single-sample color image the game aliases it with: the inverse of
// color_to_ms_depth.frag, so sample i of a pixel becomes channel i (R, G, B, A) of the same pixel. Nothing is
// averaged; a channel without a sample stays 0.

layout (binding = 0, set = 0) uniform texture2DMS depth_ms;

layout (location = 0) out vec4 out_color;

void main()
{
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const int samples = textureSamples(depth_ms);
    vec4 color = vec4(0.0);
    for (int i = 0; i < 4; ++i) {
        if (i < samples) {
            color[i] = texelFetch(depth_ms, coord, i).r;
        }
    }
    out_color = color;
}
