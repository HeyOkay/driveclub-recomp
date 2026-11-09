// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <span>
// Experimental PCSS was removed at the user's direction. Native-only contracts
// retain the accepted renderer call sites without embedding any game programs.
namespace BbSunPcss {
inline void RecordModule(vk::Device,vk::ShaderModule,std::span<const std::uint32_t>) {}
template<class Stages> inline std::array<vk::UniqueShaderModule,2> ApplyPair(vk::Device,Stages&) {return {};}
}
