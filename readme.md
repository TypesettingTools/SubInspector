# SubInspector (macOS arm64 fork)

SubInspector is a library for low level inspection and analysis of
subtitles post-rasterization.

It targets the Advanced SubStation Alpha subtitle format (ASS) and uses
[libass][libass] to parse and rasterize the subtitles.

> **This fork** adds a self-contained **macOS arm64 (Apple Silicon)** build.
> Upstream: [TypesettingTools/SubInspector][upstream]. All original code is
> subject to the upstream MIT license (see [COPYING](COPYING)); this fork's
> build changes are likewise MIT-licensed.

## Key features

- **Rasterization-accurate bounds** — measures each subtitle line's actual
  rendered bounding box (position, width, height), not font-metric guesses.
- **Visual hash (CRC32)** — every rendered line is hashed to give a measure
  of visual sameness, useful for detecting duplicate or invisible lines.
- **Solidity flag** — reports whether a line renders any opaque pixels,
  which makes "is this line actually visible" trivial to answer.
- **Per-frame timing** — `si_calculateBounds` accepts a list of timestamps,
  so `\t` animation and `\fad` fades can be measured frame by frame.
- **FFI-friendly C API** — a small, stable C interface (`si_*` functions)
  that is consumed directly from C/C++ or via LuaJIT FFI in Aegisub.
- **Self-contained dylib on macOS** — this fork statically links libass and
  its dependencies (freetype, fribidi, harfbuzz, graphite2, libpng), so the
  produced `libSubInspector.dylib` has no Homebrew dependencies.

## Requirements

- [meson][meson] >= 0.49.0 and [ninja][ninja]
- A C99 compiler (clang or MSVC 2013 Update 4+)
- libass >= 0.14.0 — either provided by the system / Homebrew, or built from
  source automatically via the `subprojects/*.wrap` fallbacks

On macOS with Homebrew:

```
brew install meson ninja libass
```

## Build

Clone the repository:

```
git clone https://github.com/ZahicAtypical/SubInspector-arm64.git
cd SubInspector-arm64
```

### macOS / Linux

```
meson build
ninja -C build
```

The result is `build/src/libSubInspector.dylib` (macOS) or
`build/src/libSubInspector.so` (Linux).

If a system libass is found (e.g. via `pkg-config`), it is used; otherwise
meson downloads and builds libass and its dependencies from the wrap files
in `subprojects/`. Caveat: the *full* wrap fallback (no system libass at
all) is fragile — on macOS it fails because libass pulls in glib, whose
meson build is broken there, and a freetype↔harfbuzz subproject recursion
can bite when neither is installed system-wide. Install libass (and ideally
freetype/harfbuzz) via your package manager first.

#### Self-contained dylib (macOS)

On macOS this fork prefers the **static** libass (`static: true` in the
meson dependency lookup), so with Homebrew installed:

```
brew install libass   # ships libass.a plus static freetype/harfbuzz/fribidi/libpng
meson build
ninja -C build
```

the resulting `libSubInspector.dylib` contains libass and its dependencies
and references only system libraries — it can be distributed to any Mac of
the same architecture. Homebrew does not ship a static `graphite2` (a
harfbuzz dependency), so `subprojects/graphite2.wrap` and an injected meson
build file under `subprojects/packagefiles/graphite2/` compile it from
source automatically; `-Wl,-dead_strip_dylibs` drops any leftover dylib
load commands.

### Windows

Requires Microsoft Visual Studio 2013 Update 4 or newer.

First, add a Windows `dirent.h` implementation somewhere on your path.
[This implementation](https://github.com/tronkko/dirent) is known to work.
See that project's README for specifics on where you can put the file.

Launch the relevant VS native tools command prompt. Be sure that if you
want a 64-bit build, you're using the one labeled x64. Then run the
following:

```
powershell
cd C:\Path\To\SubInspector
meson build
cd build
ninja
```

Should you prefer a Visual Studio solution, just pass `--backend=vs` along
with `meson build`, and then launch and build the resulting solution.

## Using with Aegisub

SubInspector ships a Lua module, `Inspector.moon`, that wraps the C library
through LuaJIT FFI. Scripts such as [ASSFoundation][assfoundation] use it to
measure subtitle bounds.

### Automatic installation (x64 platforms)

If you are on a platform with an official binary (Windows or Intel Mac),
just install any script that requires `SubInspector.Inspector` through
[DependencyControl][depcontrol] and it will fetch everything for you.

### Manual installation (Apple Silicon / arm64)

Official releases only include an `OSX-x64` binary, so on Apple Silicon
DependencyControl will fail to find a package. Build the dylib as described
above (or grab it from this fork's Releases) and place the files here:

```
~/Library/Application Support/Aegisub/automation/
└── include/
    └── SubInspector/
        ├── Inspector.moon                        ← from examples/Aegisub/
        └── Inspector/
            └── libSubInspector.dylib             ← the built dylib
```

Then in Aegisub: **Automation → Reload Scripts**. Dependency-based installs
of scripts that require `SubInspector.Inspector` will now succeed.

The directory layout matters: the module is loaded as
`requireffi('SubInspector.Inspector.SubInspector')`, which maps dots to path
segments.

## API overview

### C API

```c
#include "SubInspector.h"

SI_State *state = si_init(width, height, fontConfigPath, fontDirPath);
si_setHeader(state, header, header_len);      // [Script Info] + styles
si_setScript(state, dialogue, dialogue_len);  // one or more Dialogue lines

SI_Rect rect;
int32_t time_ms = 0;
si_calculateBounds(state, &rect, &time_ms, 1);
// rect.x, rect.y, rect.w, rect.h  → rendered bounding box
// rect.hash                        → CRC32 of the rendered bitmap
// rect.solid                       → 1 if any opaque pixel was rendered

si_cleanup(state);
```

See [`examples/C++/`](examples/C++) for a complete program that scans an
`.ass` file and reports invisible and duplicate lines.

### Lua (Aegisub) API

```lua
local Inspector = require('SubInspector.Inspector')

local inspector = Inspector(subtitles)   -- pass the aegisub subtitles table
local rects, err = inspector:getBounds(lines, times)
-- rects[i] = { x = ..., y = ..., w = ..., h = ..., hash = ..., solid = ... }
-- (one rect per render time; `times` defaults to each line's start time)
```

## Troubleshooting

- **`dyld: Library not loaded: .../libass.9.dylib`** — your dylib was built
  against a Homebrew libass that the target machine lacks. Rebuild with the
  self-contained static setup above, or install `brew install libass` there.
- **DependencyControl keeps offering to "update" SubInspector** — it matches
  the upstream feed by platform; keep the local `Inspector.moon` and ignore
  the prompt (do not let it reinstall an x64 binary).
- **Wrap download failures (savannah 502, dead forks)** — this fork already
  points the wraps at working sources (SourceForge mirror for FreeType,
  official repos for harfbuzz/zlib). If a URL rots again, drop the tarball
  into `subprojects/packagecache/` with the name in the `.wrap` file.

## Project layout

```
src/SubInspector.c        the library (single translation unit, C99)
src/SubInspector.h        public C API
examples/C++/             CLI example: find invisible/duplicate subtitle lines
examples/Aegisub/         Inspector.moon: LuaJIT FFI wrapper for Aegisub
subprojects/              wrap files for libass and its dependencies
DependencyControl.json    Aegisub DependencyControl feed definition
```

## Help and Support

Talk to `CoffeeFlux` on `irc.rizon.net` (upstream). For fork-specific build
issues, open an issue on this repository.

## License

MIT — see [COPYING](COPYING).

[libass]: https://github.com/libass/libass
[meson]: https://mesonbuild.com
[ninja]: https://ninja-build.org
[upstream]: https://github.com/TypesettingTools/SubInspector
[assfoundation]: https://github.com/TypesettingTools/ASSFoundation
[depcontrol]: https://aegi.vmoe.info/docs/3.2/Dependency_Control/
