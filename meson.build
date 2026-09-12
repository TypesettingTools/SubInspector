project('SubInspector', 'c',
    license: 'MIT',
    meson_version: '>=0.64.0',
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

# Keep bundled builds focused on subtitle rendering. These defaults can be
# overridden with -D<subproject>:<option>=<value>.
libass_options = [
    'test=disabled',
    'compare=disabled',
    'profile=disabled',
    'fuzz=disabled',
    'checkasm=disabled',
    'libunibreak=disabled',
    'freetype2:brotli=disabled',
    'freetype2:bzip2=disabled',
    'freetype2:png=enabled',
    'freetype2:zlib=enabled',
    'harfbuzz:icu=disabled',
    'harfbuzz:utilities=disabled',
    'harfbuzz:subset=disabled',
    'harfbuzz:raster=disabled',
    'harfbuzz:vector=disabled',
    'harfbuzz:gpu=disabled',
]
if host_machine.system() == 'darwin'
    libass_options += ['coretext=enabled', 'fontconfig=disabled']
elif host_machine.system() == 'windows'
    libass_options += ['directwrite=enabled', 'fontconfig=disabled']
endif

# System libass lacks the exact rasterizer scaling required by SubInspector.
libass = subproject('libass', default_options: libass_options)
deps += libass.get_variable('libass_dep')

zlib_dep = dependency('zlib', version: '>=1.2.8', required: get_option('zlib'),
                      fallback: ['zlib', 'zlib_dep'])
if zlib_dep.found()
    deps += zlib_dep
else
    add_project_arguments('-DNO_ZLIB', language: 'c')
endif

subdir('src')
subdir('tests')
