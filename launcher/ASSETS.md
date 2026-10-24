# Optional local launcher artwork

The WPF shell uses optional external files beside DriveClub.exe in assets/.
Original game images and fonts are not part of the source patch or public PR.
The separately approved app icon at launcher/wpf/Assets/app.ico is embedded as
an EXE/window resource; original DRIVECLUB logos inside the UI stay unchanged.

Required paths for the locally themed preview:

- brand/logo-red.png, brand/emblem-white.png
- fonts/001futura-webfont.ttf, fonts/004russian-webfont.ttf
- backgrounds/page-0.png (Play), page-2.png (Display), detail.png (Graphics),
  page-4.png (General), driving.png, patches.png, page-5.png (Benchmark), mods.png
- controls/option-active.png, option-inactive.png, option-current.png, panel.png
- transitions/swipe.png, transitions/stamp.png
- spinner/00.png through 28.png

Images/fonts remain local game-derived assets. Backgrounds are converted to
monochrome once on load and receive a uniform dim layer; source files are not
changed. Stationary device-pixel grain is generated procedurally. Page artwork
only crossfades; the foreground uses 280 ms slide/fade, navigation selection
220 ms, and game-folder opacity approximately 120 ms. Startup keeps the existing
swipe/stamp/logo timeline and the spinner's separate approximately 30 Hz cadence.

Read launcher/wpf/README.md for building, safe preview and capture commands.
Keep raw textures/fonts, binaries, user configurations, test packages and review
captures out of source commits. HTML/screenshot review publication is separate
from shipping or publishing original assets.
