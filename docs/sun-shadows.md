# Sun shadow quality

This development branch groups the complementary atlas, precision, depth-copy
and native-filter changes. The launcher offers a separate **Sun shadows** choice:

| Choice | Behavior |
| --- | --- |
| Game | Original game resources and filtering |
| Standard | Prepared atlas layout with qualified ray and depth-grid corrections |
| High | Higher-resolution atlas resources, retaining the native pixel filter |
| Ultra | High resources plus receiver correction and extended near-filter softness |

The default remains Game. The saved choice is preserved. These choices do not
change vehicle physics or the independent anti-aliasing selection.

Ultra's expanded softness requires **Shadow edges: Ultra**, the native 32-sample
filter. Other sample counts retain the native radius. The setting does not force
a different Shadow edges choice. The requested near-filter plateau extends to
50 metres, but per-pixel geometry and atlas guards can reduce its radius. It does
not promise identical shadow texel density at every distance.

## Preparation and fallback

The launcher prepares a separate complete cache generation. Original prepared
game files and archives are retained. Image signatures, segment and relocation
layout, patch overlap and resource profiles are checked before committing any
generation. A failed generation cannot replace a working one.

The renderer qualifies atlas ownership, synchronization, retirement and the
physical depth view before using the new path. Unknown or mixed provenance keeps
the native path. High retains the original pixel shader. Ultra replacement and
pipeline fallback restore the complete original stage pair on failure.

## Build status

The authored depth shader is generated through the normal host-shader CMake
target. The rear-allocation shim is reproduced from
`launcher/tools/rear-native-stub.s` by `generate_rear_payload.py`; it needs no game
input, and the checked-in generated header is verified against its source.

The complete locally qualified shader replacements are currently supplied as an
external development input through `BB_SUN_NATIVE_PAYLOAD_DIR`. They are not
stored in this repository. A build without that input keeps the native shader
stages and advertises only the core capability; the launcher refuses to present
it as a complete supported Sun shadows runtime. A reproducible public shader
transformation remains work in progress. This branch is not ready for upstream
publication until that transformation and the combined result are qualified.

## Validation

Seven production-helper suites cover ownership, lifecycle, coherence, resource
scope and depth provenance. Sixteen authored-shim execution cases and synthetic
preparation refusal cases cover the launcher handoff. They can be run with:

```sh
cmake -S gpu/tests/sun_shadows -B out/sun-tests -DCMAKE_BUILD_TYPE=Release
cmake --build out/sun-tests
ctest --test-dir out/sun-tests --output-on-failure
```

On Windows, run `launcher/tests/sun-shadows/run.ps1` with `-OutputDirectory` set
to a scratch directory. Actual rendering, camera motion, silhouettes and contact
shadows still need combined-build regression coverage. Success on the recorded
fixtures does not establish universal terrain-flicker elimination.
