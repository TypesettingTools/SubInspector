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

#### Windows

Requires a current Visual Studio C/C++ toolchain and NASM.

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

Should you prefer a Visual Studio solution, just pass `--backend=vs` along with `meson setup build`, and then launch and build the resulting solution.

### Help and Support

For macOS releases, see the [signing instructions](docs/releasing.md).

Talk to `CoffeeFlux` on `irc.rizon.net`.

[libass]: https://github.com/libass/libass

[releases]: https://github.com/TypesettingTools/SubInspector/releases
