# Qualified Windows build environment

The accepted source profile was built with MSYS2 CLANG64 on Windows x64. Use the CLANG64 toolchain consistently for the compiler, installed libraries, CMake, Ninja, glslang and SPIRV-Tools. The build remains offline after dependencies and the normal checkout are present; it does not fetch historical project objects or private sources.

| Tool | Qualified version |
| --- | --- |
| Clang | 23.1.3, x86_64-w64-windows-gnu, POSIX threads |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| glslang | 16.6.0 |
| SPIRV-Tools | v2026.4; MSYS2 package3~1.4.363.0-1 |

The installed CLANG64 dependency package versions were Boost1.92.0-4, fmt12.2.0-1, SDL3 3.4.18-1, FFmpeg9.0.2-1, Protobuf36.2-1, nlohmann-json3.12.0-1, OpenSSL3.6.5-1, zlib1.3.2-2, libpng1.6.59-1, miniupnpc2.3.3-4, robin-map1.4.1-1, Vulkan headers/loader1~1.4.363.0-1, Vulkan Memory Allocator3.4.0-1, xxhash0.8.4-1 and Zydis4.1.1-1. The vendored dependencies are pinned by the source manifest.

PGO defaults to the included accepted project training profile, SHA256 `3b214fc37b4ed009c6e0231ac703994288351926b7c0f6c2fd0590686f921559`, with ThinLTO. `-Pgo off` is available. Changing the toolchain, PGO or dependency versions may change output bytes and requires its own performance qualification; historical executable byte identity is not promised.

WPF packaging additionally uses the .NET8 desktop SDK. Its package restore can use an explicit offline NuGet source. Default packaging reuses the pinned DLSS provider bytes from the checkout and does not require MSVC or a DLSS SDK installation. See the README for those exact hashes and the qualified filename alias.
