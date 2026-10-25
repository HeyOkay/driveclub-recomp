# Stationary exposure check on main

Main `79fc2fe801bd0b11bf9bd16c233910b228e1f8aa`. Salar de Surire, 18:00, Clear/Dry, time compression 1x, seed 12345, 2560x1440 native DLAA, Unlocked, SDR. The car is stationary with the handbrake held in both selected images.

The sun-glare image has a nearest adapted-exposure reading of 0.010333, with limits 0.001 and 200. The away-from-sun image has exposure 0.019841, with the same bounds. These are internal game values, not nits. Readings are matched by host timestamps (approximately 11-12 ms from the saved image timestamp), not exact GPU-frame capture.

4130 stable samples over 105.7 seconds of stationary observation: exposure 0.006618-0.168809; bounds remained 0.001 and 200; no non-finite values or bound violations. The reported sky/mountain disappearance was not reproduced in these captures. This does not rule out issue #215 in other conditions; real PS4 footage is still needed to judge the expected glare.
