# DRIVECLUB WPF launcher

Every settings card and Game Files uses the measured Game Status width as its
maximum, with responsive columns and an 18 DIP gap. Quick Settings retains its
existing full-width layout and bottom docking.

Reset this page sends the current unsaved state and native page/subpage ID to the
backend. It restores only that scope; Play resets its four Quick Settings, Driving
resets the active inner tab. Other unsaved settings and game paths stay unchanged.
Player identity and language retain their previous reset exclusions. Saving remains
explicit (Save settings or Play); reset itself does not write configuration files.

Game folders retain their inline placement and responsive layout. Only their
opacity fades over 120 ms, with no height tween, page translation or popup.
Footer action columns size to content with equal 20 DIP horizontal padding.
Selected and open controls use solid accent red (#ED2B38); no burgundy gradients.
Setting captions have 6 DIP of top inset and fields remain aligned.

FilmGrainOverlay generates a single stationary noise field at the current viewport's
device-pixel dimensions with uniform alpha. It sits outside the artwork crossfade;
there are no repeating brushes, stretched grain textures or cached background tiles.

The sidebar follows upstream 0.3.1: Play, Graphics, Detail, Gameplay, Driving,
Benchmark, followed by the local Mod Manager. Cards and control order come directly from the native model. Graphics
contains frame rate/resolution, anti-aliasing and HDR. A compact game folders
block is on Play beside status in wide windows and below it in narrow windows.
Its header toggles the paths; valid folders are collapsed initially and after
selection, while invalid folders automatically expand. Collapsed paths reserve
only the header height and do not push Quick Settings down. Gameplay
contains the original gameplay and patch cards. Driving retains the native
Assists and view / Wheel and force feedback subpages. Binary community patches
remain under Gameplay. Existing styling and animations are preserved.

## Local Mod Manager

Mods imports ZIP archives or folders of loose replacement files. Paths must match
the game's `/app0` paths: for example `data/example.rpk` replaces
`/app0/data/example.rpk`. No installer scripts are executed. An optional `mod.json`
at the package root accepts `name`, `version`, `author` and `description`; a `files/`
payload directory is supported beside it. Plain `mods/` or `app0/` containers are
also accepted. Archives and folders containing arbitrary game-relative directories
retain those directories. Unsupported formats such as RAR/7z must be extracted first.

Imports start disabled. Enable/disable and Move up/down immediately update the
launcher's own `mods/` overlay for the next launch. The first enabled mod in the list
wins shared paths; the details panel identifies the winner. Original game directories
are never modified. This manages loose-file replacements, not binary patch XML or
DLL/plugin injection. Reset this page / Save settings do not apply to the Mods page.

The library and deployment journal live in `data/mod-manager/`. Existing unmanaged
files in `mods/` are preserved; a collision blocks activation until the manual file
is moved out. Externally edited managed files are also protected. File changes are
staged with backups, rolled back on failure and recovered on the next manager open
after interruption. Do not edit the overlay while applying mods or while playing.
Removing a mod disables its files and removes its entry; the imported package copy
is retained under `data/mod-manager/packages/` for recovery. The source ZIP/folder
is never altered. `DriveClub.exe --preview --mod-manager` opens this page directly.

Filesystem checks run without launching the game:
`dotnet run --project launcher/tests/mod-manager/ModManager.Tests.csproj -c Release`.

Build with `launcher/build-wpf.ps1`. Requires .NET 8 Windows Desktop runtime,
.NET SDK 8 or newer and MSYS2 CLANG64 for the native backend. Output is a separate
directory; the script does not install anything or modify an existing game build.

`DriveClub.exe` is the WPF shell; `DriveClub.Backend.exe` retains the native
settings, patch, preparation, launch and benchmark implementation. Requests and
responses use unique JSON files under the launcher's own `data/ui` folder. Long
operations run in a child process with asynchronous progress updates. Game archives
remain at the folders selected by the user; prepared files and settings belong to
this launcher's directory.

The visual layout is ported from the first launcher: 228-DIP sidebar, 36-DIP
page margins, independent card heights, original red markers, separators and
option-state textures. The logo and PC LAUNCHER sit on one line in the integrated
caption; the old PC EDITION tagline is removed.

Animations use opacity with the original smoothstep curve at twice the speed (325 ms), hardware
composition and cached artwork. Page elements use the first version's original motion: entering Y=+32 to 0,
leaving Y=0 to -12, with complementary opacity. Backgrounds only crossfade. There is no full-window GDI repaint timer. The desired animation rate is
the detected monitor refresh rate (240 Hz fallback). The original 29-frame loading
spinner advances at its own approximately 30 fps cadence. Frame callbacks can still
miss deadlines; `--perf --exit` records measured callback intervals, not hardware
presentation timestamps, in `wpf-performance.json`.

The opaque window uses custom WindowChrome with integrated caption buttons and
square corners. ComboBox and its popup share fonts, 38-DIP rows, colors and custom
templates. All settings remain standard WPF controls with accessibility names.

Original game assets are optional external local files under `assets/`. They must
never be committed or added to a public patch. Required local paths:

- `brand/logo-red.png`, `brand/emblem-white.png`
- `fonts/001futura-webfont.ttf`, `fonts/004russian-webfont.ttf`
- `backgrounds/page-0.png` through `page-6.png`: seven distinct original artworks
- `transitions/swipe.png`, `transitions/stamp.png`, `spinner/00.png` through `28.png`

Backgrounds convert to grayscale once when loaded, then receive a dark gradient;
source files remain unchanged. Three selected backgrounds are lossless stills from
the original 1920x1072 attract video; the other four are original UI artworks. The local build's `backgrounds.json` identifies each
source. No assets are embedded in either executable or referenced by the project.

`--preview` disables actual game preparation/launch and previews the loading UI.
`--capture ABSOLUTE_DIRECTORY --exit` exports all seven pages from WPF visuals.
These inspection modes do not persist settings unless Save settings is clicked.

Current chrome: 60-DIP integrated caption with separator, navigation begins 16 DIP
below it. WM_GETMINMAXINFO uses the nearest monitor work area to keep the footer
above the taskbar when maximized. Page entry and dropdown opening fade in.
Dropdowns use the same dark option texture as collapsed fields. Card accents span
the complete 44-DIP header row. Artwork includes a frozen procedural film-grain
brush; no animation/repaint timer or changes to original image files are needed.

Quick Settings uses an Auto-sized bottom row with a 24-DIP footer gap when
space permits. The upper dashboard reserves a 572-DIP minimum row height. In a
short window it pushes Quick Settings down; the entire page then scrolls together.
