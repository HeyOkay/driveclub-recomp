# Cubemap filtering previews

These six lossless PNGs accompany the seamless cubemap filtering PR. They are GPU
reconstructions of the isolated global reflection contribution from one saved Driveclub
frame, using the same material, directions, LODs and exposure for both sampling paths.
They are not two live game screenshots or a reconstruction of the complete rendering chain.

- [Before / after, 2560x1440 source](fullres/reflection-comparison.png)
- [Road crop, 100% scale](fullres/reflection-crop-100-percent.png)
- [Before, original 2560x1440](fullres/reflection-before-2560x1440.png)
- [After, original 2560x1440](fullres/reflection-after-2560x1440.png)
- [Face IDs, original 2560x1440](fullres/face-ids-2560x1440.png)
- [Earlier diagnostic contribution comparison, shared x8 display gain](reflection-filtering-before-after.png)

The full-resolution results use 3,686,400 samples. Primary face selection is unchanged;
the correction changes how the filter footprint crosses face boundaries. The comparison
previews are downscaled for display; the original PNGs are not resized. The earlier
diagnostic contribution preview uses 320x180 input and a shared x8 display gain.

Source scene: Light precipitation, observatory section. Only the global fallback specular
contribution is shown: local probes, SSR composition and the complete post-processing chain
are outside these comparisons. No raw game assets, GPU resource dumps or frame-capture
binaries are included here. The JSON files record the comparison scope and sampling counts.

