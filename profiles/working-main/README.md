# Working-main Windows source profile

This optional profile builds the working main-runtime foundation plus the native-preserving Sun transform and qualified camera source conversion. It leaves the fork's normal build and CI configuration unchanged. It does not promise the historical c15 executable's exact bytes.

Place this directory at `profiles/working-main` in the fork. Its manifest reuses pinned source/vendor files from that checkout and carries only differing sources in `overlay/`. Assembly creates a complete isolated source tree, so quoted includes cannot accidentally select modern host definitions. A changed reused file fails its pin instead of silently substituting a different ABI.

Requirements: Windows x64, MSYS2 CLANG64, Python3, Ninja/CMake, Vulkan headers/loader, glslang and SPIRV-Tools (including static core/opt/link libraries), fmt, Boost, robin-map, VMA, Zydis, xxhash, SDL3, FFmpeg, Protobuf, JSON, OpenSSL, zlib, PNG and miniupnpc. The vendored libraries remain in the normal checkout. See [the qualified toolchain and dependency versions](TOOLCHAIN.md); other installed versions may produce different executable bytes. WPF packaging additionally needs the .NET8 desktop SDK and the .NET8 desktop runtime on the target PC.

From PowerShell, using a new output directory:

```powershell
profiles/working-main/build.ps1 -Checkout . -OutputDirectory out/working-main-source
profiles/working-main/package.ps1 -SourceDirectory out/working-main-source -OutputDirectory release/working-main
```

`-ToolchainRoot` changes the MSYS2 prefix. `-Pgo off` disables PGO; the default uses the included accepted 4.4MB training profile (SHA256 `3b214fc37b4ed009c6e0231ac703994288351926b7c0f6c2fd0590686f921559`) with ThinLTO and the qualified optimization flags. `-Pgo generate -PgoDirectory <new-directory>` prepares an instrumented build; profile training is a separate deliberate gameplay operation. Turning PGO off or retraining requires fresh performance qualification. `-KeepRuntimeDebugInfo` keeps symbols in the distributed runtime; its different finalized bytes receive their own backend identity.

The accepted product modes are Standard (native), High (Standard-like filter on9216×8192) and Ultra (intermediate14336×8192 with the full near-filter transfer). High and Ultra share the100m minimum reach of the lighter texel-floor correction; Ultra near softness remains unchanged. Named quick presets below High select Standard, High selects High, Ultra selects Ultra; Custom preserves its Sun setting. See assembled `SOURCE_PROVENANCE.md` for camera and shader source provenance.

The normal build compiles all project/vendor translation units freshly. It requires no historical `.obj`, `.a`, old executable, historical absolute source path, captured Sun shader module, or generated game recompiler source. The game files are supplied through the launcher's normal user-owned game preparation flow at runtime. The authored rear shim is regenerated from its assembly; eight Sun helper libraries are generated from standalone authored GLSL and checked by spirv-val for Vulkan1.3.

Packaging finalizes the runtime (copy/strip), hashes those final bytes, then builds the backend with that identity. It copies the runtime unchanged afterward. Do any resource modification or signing before that identity step; changing the runtime afterward invalidates the backend gate. The package script builds WPF directly and does not invoke the older `build-preview` or `package.sh` routes, which would rebuild a different backend or strip the runtime too late. `-NativeOnly` omits WPF/bootstrap; start `DriveClub.Native.exe` in that package. Online server binaries are a separate normal packaging dependency; this source profile does not copy anything from an existing installation.

DLSS/DLAA and NVIDIA frame generation reuse the exact pinned DLLs already tracked in the checkout. The manifest records `packaging/dlss_bin/driveclub_dlss.dll` as an unchanged-byte alias named `bbport_dlss.dll`, which this working-main consumer loads. Its SHA256 is `5594018d55ca51d31fed24881c53880010effe2cb644a43bcaf05cd22a33ab78`; both NVIDIA DLLs are pinned too. This alias was verified for these exact bytes and is not a general future compatibility assumption. To rebuild the bridge itself, the matching `host/gpu/dlss_bridge` source documents its normal MSVC/NVIDIA SDK build. `-DlssDirectory <directory>` can explicitly package such a replacement, which requires separate qualification.

| Checkout dependency | Packaged runtime filename | Matched SHA256 |
| --- | --- | --- |
| `packaging/dlss_bin/driveclub_dlss.dll` | `bbport_dlss.dll` | `5594018d55ca51d31fed24881c53880010effe2cb644a43bcaf05cd22a33ab78` |
| `packaging/dlss_bin/nvngx_dlss.dll` | `nvngx_dlss.dll` | `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983` |
| `packaging/dlss_bin/nvngx_dlssg.dll` | `nvngx_dlssg.dll` | `ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82` |

The checked bridge provider has identical bytes to the accepted main dependency; its source header and C++ body match, with the newer provider's CMake changing its output filename. Default profile builds therefore need neither MSVC nor a DLSS SDK installation. Assembly checks the three checkout provider files against the pinned hashes before copying them.

The limited source-removal claim is explicit: nine complete captured Sun shader streams and eight bulk camera instruction bodies are absent from this profile. Short identity signatures and the dormant nf32 observer remain unchanged. No GPU behavior or performance result is implied by the CPU source-build receipt.
