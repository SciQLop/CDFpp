fs = import('fs')

flags = [
    '-lembind',
    '--bind',
    '-sWASM=1',
    '-sMODULARIZE=1',
    '-sEXPORT_NAME=createCdfModule',
    '-sALLOW_MEMORY_GROWTH=1',
    '-sEXPORT_ES6=1',
    '-sENVIRONMENT=web,worker,node',
    '-sMAXIMUM_MEMORY=4GB',
    '-fwasm-exceptions',
]

if meson.get_compiler('cpp').get_id() == 'emscripten'
    wasm_exe = executable('cdfpp',
        'wacdfpp.cpp',
        dependencies: [cdfpp_dep],
        cpp_args: ['-fwasm-exceptions'],
        link_args: flags + ['-O2'],
        install: false,
        build_by_default: true,
        extra_files: ['wasm.txt', 'wacdfpp.html']
    )

    fs.copyfile('wacdfpp.html', 'wacdfpp.html')

    node = find_program('node', required: false)
    if node.found()
        test('wasm_time_conversion', node,
            args: [
                files('../tests/wasm_time_conversion/test.mjs'),
                meson.current_build_dir() / 'cdfpp.js',
                meson.project_source_root() / 'tests' / 'resources',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_attr_detach', node,
            args: [
                files('../tests/wasm_attr_detach/test.mjs'),
                meson.current_build_dir() / 'cdfpp.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
    endif
endif
