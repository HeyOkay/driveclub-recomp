# Cubemap filtering previews

The six diagnostic PNGs below accompany the seamless cubemap filtering PR. They are GPU
reconstructions of the isolated global reflection contribution from one saved Driveclub
frame, using the same material, directions, LODs and exposure for both sampling paths.
They are not two live game screenshots or a reconstruction of the complete rendering chain.

- [Before / after, 2560x1440 source](fullres/reflection-comparison.png)
- [Seam above-left of car, 100% scale](fullres/reflection-crop-100-percent.png)
- [Before, original 2560x1440](fullres/reflection-before-2560x1440.png)
- [After, original 2560x1440](fullres/reflection-after-2560x1440.png)
- [Face IDs, original 2560x1440](fullres/face-ids-2560x1440.png)
- [Earlier diagnostic contribution comparison, shared x8 display gain](reflection-filtering-before-after.png)

The 1:1 crop covers the visible seam above-left of the car (x=720..1320, y=440..840 in the 2560x1440 source). Its pixels match the original PNGs exactly.

The full-resolution results use 3,686,400 samples. Primary face selection is unchanged;
the correction changes how the filter footprint crosses face boundaries. The comparison
previews are downscaled for display; the original PNGs are not resized. The earlier
diagnostic contribution preview uses 320x180 input and a shared x8 display gain.

Source scene: Light precipitation, observatory section. Only the global fallback specular
contribution is shown: local probes, SSR composition and the complete post-processing chain
are outside these comparisons. No raw game assets, GPU resource dumps or frame-capture
binaries are included here. The JSON files record the comparison scope and sampling counts.


## Complete game-frame comparison

- [Before: original independent array-face filtering, complete final output](full-frame/before-2560x1440.png)
- [After: native seamless cube filtering, complete final output](full-frame/after-2560x1440.png)

These two unmodified 2560x1440 PNGs are actual final-present GPU outputs from full-frame
Nsight replay with NGX/DLAA enabled, not isolated-lighting reconstructions. Both use the
same captured scene, camera, weather, resources and prior-frame history. The before replay
restores original face-local coordinates and 2D-array image types/views; the after replay
uses captured native cube shaders unchanged. They are not two newly recorded live runs.
The same restored array path is bit-identical to original array sampling in 540 GPU cases;
all 23 transformed capture shaders were validated with spirv-val. Normal replay matches
99.4052% of the captured live frame's pixels exactly; remaining replay differences are
small and recorded in full-frame/validation.json. These are controlled single-frame results,
not a separate measurement of steady-state temporal convergence.

[Full-frame road seam detail, before / after](full-frame/road-seam-zoom-before-after.png). Wide 640x420 crop above-left of the car at original pixel scale; exposure and colors are unchanged.


[Animated full-frame road detail, before / after](full-frame/road-seam-before-after.webp). Lossless WebP; 1.5 seconds per state, no blending, unchanged source pixels.
