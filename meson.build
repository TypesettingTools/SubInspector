project('SubInspector', 'c', 
    license: 'MIT',
    meson_version: '>=0.41.0',
    default_options: ['c_std=c99']
)

cc = meson.get_compiler('c')
cflags = ['-Wunreachable-code', '-Wfloat-equal', '-Wredundant-decls', '-Winit-self', '-Wpedantic']
add_project_arguments(cflags, language: 'c')

enable_zlib = get_option('enable_zlib')
b_static = get_option('static')

deps = []

deps += cc.find_library('bz2', required : false)
deps += cc.find_library('iconv', required: false)
deps += dependency('libpng', required: false)

if host_machine.system() == 'darwin'
    deps += dependency('appleframeworks', modules: ['CoreFoundation, CoreText'])
endif

if b_static
    deps_dir = [join_paths(meson.source_root(), 'deps', 'build')]
    deps += cc.find_library('fribidi', dirs: deps_dir)
    deps += cc.find_library('harfbuzz', dirs: deps_dir)
    deps += cc.find_library('freetype', dirs: deps_dir)
    deps += cc.find_library('fontconfig', dirs: deps_dir)
    deps += cc.find_library('libass', dirs: deps_dir)
else
    deps += dependency('libass', version: '>=0.14.0', static: b_static)
endif

if enable_zlib
    deps += dependency('zlib', version: '>=1.2.8', static: b_static)
else
    add_project_arguments('-DNO_ZLIB')
endif

subdir('src')
