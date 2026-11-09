// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_sun_portable.h"
#include "sun_shader_precision.h"
#include "sun_shader_transform.h"
#include <array>
#include <atomic>
#include <mutex>
namespace MainSunPortable {
namespace {
struct Memo {std::once_flag once;Program code;};
std::array<Memo,7>& Memos() {static std::array<Memo,7> memo;return memo;}
}
void Prepare(std::span<const std::uint32_t> original,Variant variant) noexcept {
    const auto index=static_cast<unsigned>(variant);
    if(index>=7) return;
    const bool fragment=variant==Variant::UltraFragment||variant==Variant::StandardFragment||variant==Variant::HighFragment;
    if(fragment?!BbSunPrecision::IsNativeFragment(original):!BbSunPrecision::IsNativeVertex(original)) return;
    try {
        auto& memo=Memos()[index];
        std::call_once(memo.once,[&] {
            // A failed first preparation is a cached refusal, not authorization.
            try {
                auto words=variant==Variant::Precision?BbSunPrecision::RewriteVertex(original):
                    variant==Variant::StandardVertex?RewriteStandardVertex(original):
                    variant==Variant::StandardFragment?RewriteStandardFragment(original):
                    variant==Variant::HighVertex?RewriteHighVertex(original):
                    variant==Variant::HighFragment?RewriteHighFragment(original):
                    fragment?RewriteQualityUltraFragment(original):RewriteQualityUltraVertex(original);
                if(!words.empty()) {
                    Program result=std::make_shared<const Words>(std::move(words));
                    std::atomic_store_explicit(&memo.code,std::move(result),std::memory_order_release);
                }
            } catch(...) {}
        });
    } catch(...) {}
}
Program Get(Variant variant) noexcept {
    const auto index=static_cast<unsigned>(variant);
    if(index>=7) return {};
    return std::atomic_load_explicit(&Memos()[index].code,std::memory_order_acquire);
}
}
