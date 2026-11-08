// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace Shader::DrawControl {
// Reuse the high bit of the motion-parameter word to stay within Vulkan's
// guaranteed 128-byte push-constant budget. Motion indices use at most 32768.
constexpr std::uint32_t TextureRepeatBit = 0x80000000u;
constexpr std::uint32_t MotionIndexMask = ~TextureRepeatBit;
constexpr std::uint32_t Pack(std::uint32_t motion, bool texture_repeat) {
    return (motion & MotionIndexMask) | (texture_repeat ? TextureRepeatBit : 0u);
}
constexpr std::uint32_t MotionIndex(std::uint32_t packed) { return packed & MotionIndexMask; }
constexpr bool TextureRepeat(std::uint32_t packed) { return (packed & TextureRepeatBit) != 0; }
}
