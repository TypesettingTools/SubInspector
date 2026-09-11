# libass rasterizer patch

SubInspector measures the bounds and hashes of rasterized subtitles. Its
previous libass dependency was the `meson-no-rasterizer-approximation` branch
of TypesettingTools/libass, whose commit
[`f1fbcfde91c2c82c7c8c50bfbd099c72d3911ac7`](https://github.com/TypesettingTools/libass/commit/f1fbcfde91c2c82c7c8c50bfbd099c72d3911ac7)
replaced an approximate rasterizer scale calculation with exact division.

`0001-exact-rasterizer-scale.patch` carries that same change against upstream
libass 0.17.5. Meson applies it when extracting the checksummed source archive.
This preserves the deliberate scaling behavior while allowing the rest of
libass to receive upstream fixes. It does not promise identical rendering
across libass versions, platforms, fonts, or changes to other dependencies.

When updating libass, review whether the upstream approximation still exists
and whether this patch remains necessary, then run the rendering tests against
the bundled build. A system-installed libass does not receive this patch.
