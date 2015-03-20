### SubInspector

SubInspector is a library for low level inspection and analysis of
subtitles post-rasterization.

It targets the Advanced SubStation Alpha subtitle format (ASS) and uses
[libass][libass] to parse and rasterize the subtitles.

### Install

To install SubInspector, download [the latest release][releases]. Bundles
are provided for Aegisub that include the SubInspector library.

### Build

1. Clone the repository with `git clone https://github.com/TypesettingCartel/SubInspector.git --recursive`.
1. If you already have the repository cloned, use `git submodule update --init --recursive`

#### Windows

Requires Microsoft Visual Studio 2013 Update 4 or newer.

1. Open SubInspector.sln and build the libSubInspector target. You must use the `Release` profile, as the `Debug` profile is broken.
1. The library is placed in `Release\<Architecture>\libSubInspector.dll`

#### Unix-like Operating Systems

Requires cmake 2.8 or newer.

1. Install your system's libass development package
1. `mkdir build && cd build` from the root of the source.
1. `cmake ..` in the build directory you just created.
1. `make`. `libSubInspector.(dylib|so)` will be placed in the build directory.

### Help and Support

Talk to `torque` on `irc.rizon.net`.

[libass]: https://github.com/libass/libass

[releases]: https://github.com/TypesettingCartel/SubInspector/releases
