render_test = executable('render-test', 'render.c',
    include_directories: include_directories('../src'),
    link_with: subinspector)
test('render', render_test)

if not meson.is_cross_build()
    luajit = find_program('luajit', required: false)
    moonc = find_program('moonc', required: false)
    if luajit.found() and moonc.found()
        wrapper = custom_target('Inspector.lua',
            input: '../examples/Aegisub/Inspector.moon',
            output: 'Inspector.lua',
            command: [moonc, '-o', '@OUTPUT@', '@INPUT@'])
        python = import('python').find_installation()
        test('aegisub-wrapper', python,
            args: [files('wrapper.py'), luajit.full_path(), wrapper,
                   subinspector.full_path(), files('wrapper.lua')],
            depends: subinspector)
    endif
endif
