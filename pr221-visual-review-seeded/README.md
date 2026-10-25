# PR #221: visual review with matching clouds

Main `79fc2fe801bd0b11bf9bd16c233910b228e1f8aa` versus PR `4a674c7bd4e38fb85b4db56c6fc87637cbf2d515`. Main is left, PR right.

Same environment seed 12345 was provided through both native and Lua config paths before environment initialization, using an identical test-only profile on each binary. Salar de Surire, 18:00, 1x time compression, native 2560x1440 DLAA, Unlocked, AI driving. Four independent 82-second runs: main/PR Clear/Dry and main/PR Cloudy/Light Precipitation. Eight camera-cycle points per run, three neighbouring captures per point, 96 lossless native PNGs captured locally.

The four JPEG sheets show the first frame at each point. Selected original 2560x1440 near-chase PNGs are in `originals/`. No exposure or display gain adjustment was applied. Independent AI trajectories, camera poses, rain particles and HUD phases still differ slightly; these are not pixel-exact comparisons. Capture-run FPS is not used for performance claims.

**Visually, I did not observe a new rendering regression in the sampled scenes.** With the same seed, the cloud layout and dominant lighting now match closely across main and PR in both weather conditions and across the sampled camera cycle. Geometry/material visibility, shadows, and body, wet-road and mirror reflections remained consistent in the inspected views. The cube LOD GPU self-test passed in both PR runs. Small remaining differences follow independent vehicle/camera motion, rain particles, wiper position and finish-line/HUD timing; neighbouring captures confirm the timing differences. This strengthens the previous visual check with matched cloud initialization, but does not establish pixel-exact equivalence.
