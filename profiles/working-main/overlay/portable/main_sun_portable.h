// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
// Separate namespace: never changes legacy BbSunUltra::registry/Kind ABI.
namespace MainSunPortable {
enum class Variant : unsigned { Precision, UltraVertex, UltraFragment, StandardVertex, StandardFragment, HighVertex, HighFragment };
using Words=std::vector<std::uint32_t>;
using Program=std::shared_ptr<const Words>;
// Synchronous preparation while the caller's original module words are live.
// Unknown inputs and any compiler/allocation/validation failure fail closed.
Words RewriteStandardVertex(std::span<const std::uint32_t> original);
Words RewriteStandardFragment(std::span<const std::uint32_t> original);
Words RewriteHighVertex(std::span<const std::uint32_t> original);
Words RewriteHighFragment(std::span<const std::uint32_t> original);
Words RewriteQualityUltraVertex(std::span<const std::uint32_t> original);
Words RewriteQualityUltraFragment(std::span<const std::uint32_t> original);
void Prepare(std::span<const std::uint32_t> original,Variant variant) noexcept;
Program Get(Variant variant) noexcept;
}
