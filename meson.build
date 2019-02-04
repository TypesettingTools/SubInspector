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

deps += dependency('libass', version: '>=0.14.0',
                   fallback: ['libass', 'libass_dep'])

zlib_dep = dependency('zlib', version: '>=1.2.8', required: get_option('zlib'),
                      fallback: ['zlib', 'zlib_dep'])
if zlib_dep.found()
    deps += zlib_dep
else
    add_project_arguments('-DNO_ZLIB')
endif

subdir('src')
