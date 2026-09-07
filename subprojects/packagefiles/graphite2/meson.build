# Minimal meson build for graphite2 1.3.14 (upstream ships CMake only).
# Injected via subprojects/packagefiles/graphite2/ — see graphite2.wrap.
# Mirrors the upstream CMake configuration used on macOS:
#   - call VM machine (direct requires GCC; AppleClang falls back to call)
#   - tracing compiled out (GRAPHITE2_NTRACING, upstream default)
#   - static library (GRAPHITE2_STATIC)
project('graphite2', 'cpp',
    version: '1.3.14',
    license: 'LGPL-2.1-or-later',
    default_options: ['cpp_std=c++11'])

graphite2_sources = files(
    'src/call_machine.cpp',
    'src/FileFace.cpp',
    'src/gr_char_info.cpp',
    'src/gr_features.cpp',
    'src/gr_face.cpp',
    'src/gr_font.cpp',
    'src/gr_logging.cpp',
    'src/gr_segment.cpp',
    'src/gr_slot.cpp',
    'src/CmapCache.cpp',
    'src/Code.cpp',
    'src/Collider.cpp',
    'src/Decompressor.cpp',
    'src/Face.cpp',
    'src/FeatureMap.cpp',
    'src/Font.cpp',
    'src/GlyphFace.cpp',
    'src/GlyphCache.cpp',
    'src/Intervals.cpp',
    'src/Justifier.cpp',
    'src/NameTable.cpp',
    'src/Pass.cpp',
    'src/Position.cpp',
    'src/Segment.cpp',
    'src/Silf.cpp',
    'src/Slot.cpp',
    'src/Sparse.cpp',
    'src/TtfUtil.cpp',
    'src/UtfCodec.cpp',
)

graphite2_includes = include_directories('include', 'src', 'src/inc')

graphite2_lib = static_library('graphite2',
    graphite2_sources,
    include_directories: graphite2_includes,
    cpp_args: ['-DGRAPHITE2_STATIC', '-DGRAPHITE2_NTRACING', '-fno-rtti', '-fno-exceptions'],
    install: false)

graphite2_dep = declare_dependency(
    link_with: graphite2_lib,
    include_directories: graphite2_includes)
