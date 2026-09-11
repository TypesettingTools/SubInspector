### SubInspector

SubInspector is a library for low level inspection and analysis of
subtitles post-rasterization.

It targets the Advanced SubStation Alpha subtitle format (ASS) and uses
[libass][libass] to parse and rasterize the subtitles.

### Install

To install SubInspector, download [the latest release][releases]. Bundles
are provided for Aegisub that include the SubInspector library.

### Build

Requires Meson version 0.64.0 or higher, Ninja, and a C/C++ compiler.

1. Clone the repository with `git clone https://github.com/TypesettingTools/SubInspector.git`.

#### Unix-like Operating Systems

Building on OSX and Linux should be as simple as running the following:
```
cd /path/to/SubInspector
meson setup build
cd build
ninja
```

The default build uses installed dependencies when available. To build the
pinned dependencies from source on macOS (Intel or Apple Silicon), including
SubInspector's libass rasterizer patch, use:

```
meson setup build-bundled --wrap-mode=forcefallback -Dauto_features=disabled \
  -Dzlib=enabled -Dfreetype2:zlib=enabled -Dfreetype2:png=enabled \
  -Dlibass:coretext=enabled -Dlibass:asm=enabled \
  -Dharfbuzz:utilities=disabled -Dharfbuzz:subset=disabled \
  -Dharfbuzz:raster=disabled -Dharfbuzz:vector=disabled -Dharfbuzz:gpu=disabled
meson compile -C build-bundled
meson test -C build-bundled --print-errorlogs
```

Intel builds require NASM for libass's assembly routines. The resulting
`build-bundled/src/libSubInspector.dylib` embeds the third-party dependencies
and uses macOS's Core Text font provider. Check its runtime dependencies with
`otool -L`; a distributable build should reference only system libraries and
frameworks. The binary's architecture must match the Aegisub process loading it.

Dependency releases and archive hashes are pinned in `subprojects/*.wrap`.
The libass patch retains the exact rasterizer scaling used by SubInspector's
previous libass fork; see [the patch notes](subprojects/packagefiles/libass/README.md).
Using a system libass bypasses this patch and can produce different bounds
and hashes. Dependency updates can also change rasterization; hashes should
not be compared across library builds.

Run the rendering tests for any build with `meson test -C build --print-errorlogs`.

#### Windows

Use a current Visual Studio C/C++ toolchain and NASM when building the pinned
dependencies. libass now supplies its own Windows directory handling.

Launch the relevant VS native tools command prompt. Be sure that if you
want a 64-bit built, you're using the one labeled x64. Then run the
following:
```
powershell
cd C:\Path\To\SubInspector
meson setup build
cd build
ninja
```

For a bundled Windows build, use the source-build options above with
`-Dlibass:directwrite=enabled` in place of `-Dlibass:coretext=enabled`, and add
`-Db_vscrt=mt` to link the MSVC runtime statically.

### Help and Support

Talk to `CoffeeFlux` on `irc.rizon.net`.

[libass]: https://github.com/libass/libass

[releases]: https://github.com/TypesettingTools/SubInspector/releases
