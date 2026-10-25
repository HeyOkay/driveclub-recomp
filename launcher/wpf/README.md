# DRIVECLUB WPF launcher

Windows .NET 8 WPF shell over the native launcher backend. `DriveClub.exe` is the
shell; `DriveClub.Backend.exe` owns settings, patch selection, game preparation,
launch and benchmarks. This change does not replace game or renderer logic.

## Settings

- **Play:** game status and folders; Quick Settings in the order Performance
  preset, Frame rate, Output resolution, Anti-aliasing.
- **Display:** frame rate, resolution/window, anti-aliasing/filtering and HDR.
- **Graphics:** scene detail, reflections and camera effects, including the
  existing Slow/Fast motion-blur exposure control.
- **General:** player profile, content, startup/overlays and advanced launcher options.
- **Driving:** Assists and view / Wheel and force feedback subpages.
- **Patches:** compatibility workarounds, gameplay changes, photo mode, effect
  overrides. Each existing optional patch retains
  its own control.
- **Benchmark:** the native AI-driven repeatable event and results.
- **Mods:** local loose-file replacements; not a downloadable mod catalogue.

The native card model is the single source of category membership and reset
scopes. Controls omitted by the native release are not exposed by the shell.
Resolution presets and the existing settings/patch formats are retained.

### PS4 preset

Quick Settings offers **PS4**: 30 FPS, native 1080p output/render, original game
anti-aliasing and texture filtering, live mirrors, original model/scenery/shadow
quality, dynamic resolution, motion blur with Slow exposure, lens dust, heat haze
and FXAA. It restores 100% field of view and disables optional gameplay/photo
mode/effect overrides. Player identity, language, game folders, driving assists,
wheel settings, content and startup preferences are preserved.

The PS4 preset disables the camera/replay workaround: it is unnecessary at
30 FPS. Explicitly selecting 60 FPS delta time or Unlocked/VRR enables it;
selecting 30 FPS or fixed tick selects its off default. It remains individually
switchable in Patches. A manual opt-out survives save/restart and other setting
changes, until the next explicit FPS-mode change. Old configurations without a
camera preference derive it from their saved frame mode; existing preferences
are retained.

The runtime contract matches PR #187: camera_patch=0/1 in launcher.ini and
BB_CAMERA_PATCH=0/1 for the game. The implementation remains in that separate
PR. Unsupported runtimes show an unavailable control with an explanation.
Selecting a profile does not install a runtime patch.

Reset this page affects only the current page or active Driving subpage. Play
resets its four Quick Settings. Identity/language retain their exclusions.
Reset and profile selection do not save; Save settings and Play persist changes.

## Build and review

Requires Windows, MSYS2 CLANG64, .NET SDK 8+ with Windows desktop targeting, and
.NET 8 Windows Desktop runtime for the framework-dependent shell.

```powershell
./launcher/build-wpf.ps1 -OutputDirectory C:\path\to\separate\preview
python launcher/tests/backend/verify.py C:\path\to\preview\DriveClub.Backend.exe C:\path\to\test-output
python launcher/tests/backend/preservation.py C:\path\to\preview\DriveClub.Backend.exe C:\path\to\existing-install C:\path\to\private-test-output
dotnet run --project launcher/tests/mod-manager/ModManager.Tests.csproj -c Release
```

Build outputs go into a separate directory. To run normally, provide the release
runtime, native helper applications/licenses and game folders beside the shell.
The theme is copied automatically from `launcher/assets/` during both build
and publish. This script does not replace an installed game.
The normal upstream build path remains available; this is a separate Windows
frontend build until packaging is explicitly adopted upstream.

`DriveClub.exe --preview` prevents auto-play and previews the loading sequence
when Play/Benchmark is pressed; no game is prepared or launched. Settings remain
editable in this executable's own directory. `--preview --capture ABSOLUTE_PATH
--exit` exports actual WPF pages, full settings canvases and layout diagnostics.
`--compact` and `--maximized` exercise window bounds. `--perf --exit` reports WPF
render callback intervals; they are not GPU presentation timestamps.

## Visual design and assets

Opaque WindowChrome window, 228-DIP neutral sidebar, accent #ED2B38, integrated
caption/footer, original game logos and bundled fonts/artwork. Cards
use the measured Game Status width, centered within safe margins, with an 18-DIP
column gap. Minimum width accommodates two cards. Long captions wrap. Quick
Settings stays at the bottom when space permits; the whole page scrolls when
upper content needs more space. Valid game folders collapse; invalid ones expand.

Page transitions use 280 ms vertical slide/fade; the sidebar selection moves over
220 ms while labels stay still. Artwork only crossfades. The original startup
animation and 29-frame spinner retain their independent timelines. Uniform,
stationary grain is generated at device-pixel resolution with no tiling.

The complete reviewed theme is tracked in `launcher/assets/` and copied to
`assets/` beside the executable by the WPF project, including on publish. No
manual theme extraction is needed. Keep this directory in the release package.
See `launcher/ASSETS.md` for file layout, provenance and ownership notes.
`wpf/Assets/app.ico` is the shared PC app/game icon. Captures, user settings,
runtime binaries and example mods remain outside source commits.

## Local Mod Manager

Imports folders or ZIP archives of loose files, with optional mod.json metadata.
Containers files/, mods/ and app0/ are accepted; arbitrary game-relative paths
are preserved. Imports start disabled. The highest enabled entry wins conflicts.
Enable/disable/reorder apply immediately to the launcher's own mods/ directory.
Save/reset do not apply to this page.

Original game archives and import sources are not modified. The library and
journal live in data/mod-manager/; packages are retained for recovery after
removal. Traversal, symlink/reparse, Windows device-name and case-collision
checks protect imports. Deployment protects unmanaged/externally edited files,
uses staging/rollback and recovers an interrupted journal on restart. No actual
in-game mods were verified by the launcher filesystem tests.

## Existing user configurations

The shell and backend use the existing launcher.ini, patch selection and user
sidecars beside the launcher. Opening the shell does not select the PS4 preset.
Saving retains unknown launcher/FFB keys, camera/HDR/overlay sidecar keys and
unlisted custom patch selections. The two-thread control owns only bit 37 of
an existing optimization mask and preserves all unrelated bits. Invalid saved
masks reject Save instead of replacing the file with defaults. Text updates use
a temporary file and atomic replacement so a failed replacement retains the
previous file. These guarantees are tested against copies, never by modifying
the supplied existing installation. Native pending wheel-setting migration is
retained and affects only recognized wheel keys.

## Temporary workaround lifecycle

Temporary patches are explicitly grouped under **Temporary workarounds**, named
as workarounds, described as experimental where appropriate and individually
toggleable. They must not be presented as permanent native fixes. The current
camera workaround still follows the explicit FPS-mode defaults described above;
a saved manual opt-out remains respected.

When the underlying runtime/core issue is fixed, verify the affected scenarios
with the workaround disabled. Once that native correction is established, retire
the workaround's automatic enabling and UI entry instead of stacking both paths.
Keep old config keys tolerated for compatibility. Update the source condition,
capability handling, descriptions and tests together. A runtime version number
alone does not prove the issue is fixed; this launcher does not automatically
claim to detect an unverified native correction.
