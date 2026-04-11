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
endif
