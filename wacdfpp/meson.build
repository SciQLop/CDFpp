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
    '-fwasm-exceptions',
    # getExceptionMessage() for C++ exceptions that reach JS undecoded (convert-worker.js)
    '-sEXPORT_EXCEPTION_HANDLING_HELPERS=1',
]

# A wasm64 build (cross file wasm64.txt) is the same module with 64-bit memory, for files whose
# decoded values do not fit in wasm32's 4 GiB. Browsers cap 64-bit memory at 16 GiB.
wasm64 = host_machine.cpu_family() == 'wasm64'
wasm_name = wasm64 ? 'cdfpp64' : 'cdfpp'
flags += wasm64 ? ['-sMEMORY64=1', '-sMAXIMUM_MEMORY=16GB'] : ['-sMAXIMUM_MEMORY=4GB']

if meson.get_compiler('cpp').get_id() == 'emscripten'
    wasm_exe = executable(wasm_name,
        'wacdfpp.cpp',
        dependencies: [cdfpp_dep],
        cpp_args: ['-fwasm-exceptions'],
        link_args: flags + ['-O2'],
        install: false,
        build_by_default: true,
        extra_files: ['wasm.txt', 'wasm64.txt', 'wacdfpp.html', 'app.js', 'cdf-model.js', 'render.js',
                      'plot-model.js', 'spectrogram.js', 'plot.js', 'astralint.js',
                      'wasm.js', 'cdf-diff.js', 'compare.js', 'cdf-yaml.js', 'jsdiff.js',
                      'convert.js', 'convert-model.js', 'convert-worker.js',
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
    fs.copyfile('cdf-yaml.js', 'cdf-yaml.js')
    fs.copyfile('convert.js', 'convert.js')
    fs.copyfile('convert-model.js', 'convert-model.js')
    fs.copyfile('convert-worker.js', 'convert-worker.js')
    fs.copyfile('jsdiff.js', 'jsdiff.js')
    fs.copyfile('uPlot.esm.js', 'uPlot.esm.js')
    fs.copyfile('uPlot.min.css', 'uPlot.min.css')

    node = find_program('node', required: false)
    node_runs_module = node.found()
    if node_runs_module and wasm64
        # wasm64 modules need 64-bit memories *and* tables: Node 22 has the first but not the second.
        node_runs_module = run_command(node, '-e',
            'const m = (s) => WebAssembly.validate(new Uint8Array([0,97,115,109,1,0,0,0,...s]));' +
            'process.exit(m([5,3,1,4,0]) && m([4,4,1,0x70,4,0]) ? 0 : 1)',
            check: false).returncode() == 0
    endif
    if node_runs_module
        test('wasm_time_conversion', node,
            args: [
                files('../tests/wasm_time_conversion/test.mjs'),
                meson.current_build_dir() / wasm_name + '.js',
                meson.project_source_root() / 'tests' / 'resources',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_attr_detach', node,
            args: [
                files('../tests/wasm_attr_detach/test.mjs'),
                meson.current_build_dir() / wasm_name + '.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_save_roundtrip', node,
            args: [
                files('../tests/wasm_save_roundtrip/test.mjs'),
                meson.current_build_dir() / wasm_name + '.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_convert', node,
            args: [
                files('../tests/wasm_convert/test.mjs'),
                meson.current_build_dir() / wasm_name + '.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
        test('wasm_time_attrs', node,
            args: [
                files('../tests/wasm_time_attrs/test.mjs'),
                meson.current_build_dir() / wasm_name + '.js',
            ],
            depends: wasm_exe,
            timeout: 120,
        )
    endif
endif
