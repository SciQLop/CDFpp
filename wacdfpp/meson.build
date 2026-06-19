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
        extra_files: ['wasm.txt', 'wacdfpp.html', 'app.js', 'cdf-model.js', 'render.js',
                      'plot-model.js', 'spectrogram.js', 'plot.js', 'astralint.js',
                      'wasm.js', 'cdf-diff.js', 'compare.js',
                      'uPlot.esm.js', 'uPlot.min.css']
    )

    fs.copyfile('wacdfpp.html', 'wacdfpp.html')
    fs.copyfile('app.js', 'app.js')
    fs.copyfile('cdf-model.js', 'cdf-model.js')
    fs.copyfile('render.js', 'render.js')
    fs.copyfile('plot-model.js', 'plot-model.js')
    fs.copyfile('spectrogram.js', 'spectrogram.js')
    fs.copyfile('plot.js', 'plot.js')
    fs.copyfile('astralint.js', 'astralint.js')
    fs.copyfile('wasm.js', 'wasm.js')
    fs.copyfile('cdf-diff.js', 'cdf-diff.js')
    fs.copyfile('compare.js', 'compare.js')
    fs.copyfile('uPlot.esm.js', 'uPlot.esm.js')
    fs.copyfile('uPlot.min.css', 'uPlot.min.css')

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
        test('wasm_save_roundtrip', node,
            args: [
                files('../tests/wasm_save_roundtrip/test.mjs'),
                meson.current_build_dir() / 'cdfpp.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_time_attrs', node,
            args: [
                files('../tests/wasm_time_attrs/test.mjs'),
                meson.current_build_dir() / 'cdfpp.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
    endif
endif
