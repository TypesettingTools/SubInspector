### SubInspector

SubInspector is a library for low level inspection and analysis of
subtitles post-rasterization.

It targets the Advanced SubStation Alpha subtitle format (ASS) and uses
[libass][libass] to parse and rasterize the subtitles.

### Install

To install SubInspector, download [the latest release][releases]. Bundles
are provided for Aegisub that include the SubInspector library.

### Build

Requires meson version 0.49.0 or higher.

1. Clone the repository with `git clone https://github.com/TypesettingTools/SubInspector.git`.

#### Unix-like Operating Systems

Building on OSX and Linux should be as simple as running the following:
```
cd /path/to/SubInspector
meson build
cd build
ninja
```

#### Windows

Requires Microsoft Visual Studio 2013 Update 4 or newer.

First, add a Windows `dirent.h` implementation somewhere on your path.
[This implementation](https://github.com/tronkko/dirent) is known to work.
See that project's README for specifics on where you can put the file.

Launch the relevant VS native tools command prompt. Be sure that if you
want a 64-bit built, you're using the one labeled x64. Then run the
following:
```
powershell
cd C:\Path\To\SubInspector
meson build
cd build
ninja
```

Should you prefer a Visual Studio solution, just pass `--backend=vs` along with `meson build`, and then launch and build the resulting solution.

### Help and Support

Talk to `CoffeeFlux` on `irc.rizon.net`.

[libass]: https://github.com/libass/libass

[releases]: https://github.com/TypesettingTools/SubInspector/releases
