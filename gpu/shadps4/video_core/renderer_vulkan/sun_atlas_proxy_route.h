#pragma once
#include <cstddef>
namespace Vulkan::SunAtlasProxy {
// Inspect only the completed RenderState's active slots. A shader MRT bit can
// remain set when native color binding is disabled and the slot is already
// null. Preserve the pipeline, attachment count, and every slot exactly as
// supplied.
template <class Attachment>
bool AllNullColorViews(const Attachment* attachments, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (attachments[i].image_view)
            return false;
    return true;
}
} // namespace Vulkan::SunAtlasProxy
