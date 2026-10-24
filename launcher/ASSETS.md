# Original-game launcher theme

The launcher uses the existing Win32/GDI+ backend, settings files, preparation and launch logic. Its sidebar separates Play, General, Graphics, Performance, Gameplay, Benchmark and Advanced. Quick settings on Play edit the same model as the full settings pages.

The theme is optional. Without game images/fonts it uses painted backgrounds and system fonts. No proprietary assets are embedded in the executable or tracked in Git.

## Local assets

Place your locally extracted theme beside the executable in `assets/`, or set `DRIVECLUB_ASSET_DIR` to that directory. Layout:

```
assets/
  brand/logo-red.png                 # boot.rpk :: dc_logo_red.tif
  brand/emblem-white.png             # shared_assets.rpk :: driveclub_logo_small_white.tif
  backgrounds/menu-car.png           # shared_assets.rpk :: background_car_grey.tif
  backgrounds/loading-cars.png       # loading.rpk :: loading_screen_fge.tif
  controls/option-active.png         # shared_assets.rpk :: options_item_active.tif
  controls/option-inactive.png       # shared_assets.rpk :: options_item_inactive.tif
  controls/option-current.png        # shared_assets.rpk :: options_item_current.tif
  controls/panel.png                 # shared_assets.rpk :: options_panel_background.tif
  transitions/swipe.png              # patch-dcbikes.rpk :: swipe.tif
  transitions/stamp.png              # patch-dcbikes.rpk :: stamp.tif
  spinner/00.png ... 28.png          # shared_assets.rpk :: loading_icon_00000 ... 00028.tif
  fonts/001futura-webfont.ttf
  fonts/004russian-webfont.ttf
  fonts/stratumuniversal-webfont.ttf
```

Use the logical dimensions after GPU detiling and crop storage padding. Mipmapped resources have power-of-two storage dimensions, with each mip allocated in at least an 8×8 tile of elements. For example the 676×88 RGBA8 option textures occupy a 1024×128 base allocation. Detiling at logical pitch produces corrupted stripes.

## Transitions

Page changes use a 650 ms fade and short vertical slide, adapted from `SlideUpIn` for desktop navigation. The sidebar/footer stay fixed. Time comes from QueryPerformanceCounter; the timer only requests repaints. Rapid page changes replace the outgoing layer safely.

The preparation view uses the original swipe/stamp dependencies and a 1.6 s sequence: swipe/logoDrop at 0.1 s, logo fade at 1.1 s, end at 1.6 s. The 29 original spinner frames advance every 33 ms. The engine's exact `smooth`/`smooth-smooth` equations were not recovered; this renderer uses cubic smoothstep. RTT/blur and perspective are adapted to 2D.

## Build and inspect

From PowerShell, with MSYS2 CLANG64 installed:

```
./launcher/build-preview.ps1 -OutputDirectory C:\path\to\separate\preview
```

This builds only the launcher. It does not run the package script, replace an installed game or write BAT files. The output directory needs its own `runtime/` and settings for normal play. The existing release build command also compiles `launcher_theme.inc` through `driveclub_launcher.cpp`.

- `DriveClub.exe --preview`: interactive UI; Play and Run benchmark show a temporary preparation preview without preparing or launching the game. Settings changes still belong to this executable's own folder.
- `DriveClub.exe --snapshot C:\path\image.png`: export the actual renderer. Optional environment variables: `LAUNCHER_SNAPSHOT_TAB` (0–6), `LAUNCHER_SNAPSHOT_W`, `LAUNCHER_SNAPSHOT_H`, `LAUNCHER_SNAPSHOT_SCROLL` (logical pixels).
- `DriveClub.exe --ui-film C:\path\frames`: deterministic PNG sequence from the native renderer, for reviewing transitions without a game process.

Tab / Shift+Tab move focus; Enter / Space activate; Escape removes the focus indicator. Dropdown choices and language selection use native owner-drawn menus. Benchmark track/car menus retain their existing location/class grouping.

Keep original images, fonts, captured screenshots/video and user settings out of public patches and PRs. Include only source and documentation.
