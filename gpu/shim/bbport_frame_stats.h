// Rolling present-time statistics for the overlay. No game memory or GPU
// readbacks.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <vector>

namespace BbOverlay {
class FrameTimingStats {
public:
  using Clock = std::chrono::steady_clock;
  struct Lows {
    bool ready = false;
    float one = 0, point_one = 0;
  };
  void Add(Clock::time_point now, float ms) {
    if (!(ms > 0) || !std::isfinite(ms))
      return;
    while (!frames.empty() &&
           now - frames.front().time > std::chrono::seconds(60))
      frames.pop_front();
    frames.push_back({now, ms});
  }
  Lows Get(Clock::time_point now) {
    if (frames.empty() || now - frames.front().time < std::chrono::seconds(10))
      return {};
    scratch.clear();
    for (const auto &f : frames)
      scratch.push_back(f.ms);
    return {true, Slowest(0.01), Slowest(0.001)};
  }

private:
  struct Frame {
    Clock::time_point time;
    float ms;
  };
  std::deque<Frame> frames;
  std::vector<float> scratch;
  float Slowest(double fraction) {
    const size_t count =
        std::max(size_t(1), size_t(std::ceil(scratch.size() * fraction)));
    if (count < scratch.size())
      std::nth_element(scratch.begin(), scratch.begin() + count, scratch.end(),
                       std::greater<float>());
    double total = 0;
    for (size_t i = 0; i < count; ++i)
      total += scratch[i];
    return float(1000.0 * count / total);
  }
};
} // namespace BbOverlay
