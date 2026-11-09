// SPDX-License-Identifier: GPL-2.0-or-later
// PRIVATE Standard native-atlas experiment; canonical High/Ultra helpers unchanged.
#include "main_sun_portable.h"
#include "sun_shader_transform.h"
#include "video_core/host_shaders/sun_standard_vertex.h"
#include "video_core/host_shaders/sun_standard_fragment.h"
namespace MainSunPortable {
Words RewriteStandardVertex(std::span<const std::uint32_t> original) {
 return BbSunPortable::RewriteUltraVertex(original,std::span{SUN_STANDARD_VERTEX});
}
Words RewriteStandardFragment(std::span<const std::uint32_t> original) {
 return BbSunPortable::RewriteUltraFragment(original,std::span{SUN_STANDARD_FRAGMENT});
}
}
