render_test = executable('render-test', 'render.c',
    include_directories: include_directories('../src'),
    link_with: subinspector)
test('render', render_test)

if not meson.is_cross_build()
    python = import('python').find_installation()
    if add_languages('cpp', required: false, native: false)
        example = executable('si-example', '../examples/C++/example.cpp',
            override_options: ['cpp_std=c++11'], link_with: subinspector)
        test('example-line-endings', python,
            args: [files('example.py'), example.full_path()], depends: example)
    endif
    luajit = find_program('luajit', required: false)
    moonc = find_program('moonc', required: false)
    if luajit.found() and moonc.found()
        wrapper = custom_target('Inspector.lua',
            input: '../examples/Aegisub/Inspector.moon',
            output: 'Inspector.lua',
            command: [moonc, '-o', '@OUTPUT@', '@INPUT@'])
        test('aegisub-wrapper', python,
            args: [files('wrapper.py'), luajit.full_path(), wrapper,
                   subinspector.full_path(), files('wrapper.lua')],
            depends: subinspector)
    endif
endif
