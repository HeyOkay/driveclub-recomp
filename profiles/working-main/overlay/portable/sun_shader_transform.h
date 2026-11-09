// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace BbSunPortable {
// Unknown input, failed helper linkage or invalid output returns an empty vector.
std::vector<std::uint32_t> RewriteUltraVertex(std::span<const std::uint32_t> native);
std::vector<std::uint32_t> RewriteUltraFragment(std::span<const std::uint32_t> native);
// Explicit authored libraries support offline tests and reproducible toolchains.
std::vector<std::uint32_t> RewriteUltraVertex(std::span<const std::uint32_t> native,
                                            std::span<const std::uint32_t> helper);
std::vector<std::uint32_t> RewriteUltraFragment(std::span<const std::uint32_t> native,
                                              std::span<const std::uint32_t> helper);
} // namespace BbSunPortable
