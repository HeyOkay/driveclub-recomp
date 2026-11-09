// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_sun_portable.h"
#include "sun_shader_transform.h"
#include "video_core/host_shaders/sun_high_vertex.h"
#include "video_core/host_shaders/sun_high_fragment.h"
#include "video_core/host_shaders/sun_ultra_vertex.h"
#include "video_core/host_shaders/sun_ultra_fragment.h"
namespace MainSunPortable {
Words RewriteHighVertex(std::span<const std::uint32_t> original){return BbSunPortable::RewriteUltraVertex(original,std::span{SUN_HIGH_VERTEX});}
Words RewriteHighFragment(std::span<const std::uint32_t> original){return BbSunPortable::RewriteUltraFragment(original,std::span{SUN_HIGH_FRAGMENT});}
Words RewriteQualityUltraVertex(std::span<const std::uint32_t> original){return BbSunPortable::RewriteUltraVertex(original,std::span{SUN_ULTRA_VERTEX});}
Words RewriteQualityUltraFragment(std::span<const std::uint32_t> original){return BbSunPortable::RewriteUltraFragment(original,std::span{SUN_ULTRA_FRAGMENT});}
}
