#include "../gpu/shim/bbport_frame_stats.h"
#include <cassert>
#include <cmath>
using BbOverlay::FrameTimingStats;
int main() {
  FrameTimingStats stats;
  const auto origin = FrameTimingStats::Clock::time_point{};
  // 1000 presents over ten seconds: 10 slow frames (nine at 20ms, one at 40ms).
  for (int i = 0; i < 1000; ++i)
    stats.Add(origin + std::chrono::milliseconds(i * 10), i == 999   ? 40.0f
                                                          : i >= 990 ? 20.0f
                                                                     : 10.0f);
  assert(!stats.Get(origin + std::chrono::milliseconds(9990)).ready);
  auto lows = stats.Get(origin + std::chrono::seconds(10));
  assert(lows.ready && std::abs(lows.one - 10000.0f / 220.0f) < 0.001f);
  assert(std::abs(lows.point_one - 25.0f) < 0.001f);
  // A new session beyond the rolling window cannot inherit old stalls or
  // warm-up.
  stats.Add(origin + std::chrono::seconds(80), 5.0f);
  assert(!stats.Get(origin + std::chrono::seconds(80)).ready);
  for (int i = 1; i <= 1000; ++i)
    stats.Add(origin + std::chrono::seconds(80) +
                  std::chrono::milliseconds(i * 10),
              5.0f);
  lows = stats.Get(origin + std::chrono::seconds(90));
  assert(lows.ready && lows.one == 200.0f && lows.point_one == 200.0f);
}
