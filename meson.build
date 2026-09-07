project('SubInspector', 'c',
    license: 'MIT',
    meson_version: '>=0.49.0',
    version: '0.5.2',
    default_options: ['c_std=c99', 'default_library=static', 'buildtype=release']
)

cc = meson.get_compiler('c')
if cc.get_argument_syntax() == 'gcc'
    cflags = ['-Wunreachable-code', '-Wfloat-equal', '-Wredundant-decls', '-Winit-self', '-Wpedantic']
    add_project_arguments(cflags, language: 'c')
endif

deps = []

deps += cc.find_library('bz2', required: false)
deps += cc.find_library('iconv', required: false)

# Homebrew ships no static graphite2 (a harfbuzz dependency), so on macOS we
# always build it from source via subprojects/graphite2.wrap and link it
# *before* the libass/harfbuzz pkg-config flags (whose -lgraphite2 would
# otherwise resolve to the Homebrew dylib). -dead_strip_dylibs then drops the
# unused dylib load command and the result stays self-contained.
if host_machine.system() == 'darwin'
    graphite2_sub = subproject('graphite2')
    deps += declare_dependency(
        link_args: ['-Wl,-dead_strip_dylibs'],
        dependencies: [graphite2_sub.get_variable('graphite2_dep')])
endif

deps += dependency('libass', version: '>=0.14.0', static: true,
                   fallback: ['libass', 'libass_dep'])

zlib_dep = dependency('zlib', version: '>=1.2.8', required: get_option('zlib'),
                      fallback: ['zlib', 'zlib_dep'])
if zlib_dep.found()
    deps += zlib_dep
else
    add_project_arguments('-DNO_ZLIB')
endif

subdir('src')
