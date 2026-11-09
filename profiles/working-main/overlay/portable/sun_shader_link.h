// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>
namespace BbSunPortable {
struct LinkedSymbol { std::uint32_t id{}, type{}; };
struct LinkPlan {
    std::vector<std::uint32_t> native, library;
    // Private global symbols expose their value type; functions expose return void.
    std::map<std::string, LinkedSymbol> symbols;
    std::uint32_t AllocateId();
};
// Prepares standalone authored GLSL helper libraries for linkage to caller-owned
// native words. Throws on unsupported/ambiguous ABI; never mutates caller input.
LinkPlan PrepareLink(std::span<const std::uint32_t> native,
                     std::span<const std::uint32_t> authored_helper);
// Caller inserts calls and transfers into plan.native using the returned symbols.
// Resolves IDs/types, restores the complete entry interface and validates Vulkan1.3.
std::vector<std::uint32_t> FinishLink(LinkPlan plan);
}
