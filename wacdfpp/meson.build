fs = import('fs')


flags = ['-sWASM=2', '-sALLOW_MEMORY_GROWTH', '-sFETCH=1', '-sASYNCIFY']+['-sWASM=2', '-sEXPORT_ALL=1', '-sMODULARIZE', '-sEXPORTED_RUNTIME_METHODS=ccall', '-lembind', '-sFETCH=1', '--bind', '-sALLOW_MEMORY_GROWTH']

lib_wacdfpp = library('wacdfpp', 'wacdfpp.cpp',
                    dependencies:[cdfpp_dep],
                    cpp_args : flags,
                    install: false,
                    link_args : flags,
                    build_by_default: meson.get_compiler('cpp').get_id() == 'emscripten',
                    extra_files : ['wasm.txt', 'wacdfpp.html']
)

if meson.get_compiler('cpp').get_id() == 'emscripten'
    js_wrapper = custom_target('js_wrapper',
        input : lib_wacdfpp,
        output : 'wacdfpp.js',
        command : [ 'emcc', '-lembind', '-sFETCH=1', '-sWASM=2',  '--bind', '-sALLOW_MEMORY_GROWTH',  '-sASYNCIFY', '-o', '@OUTPUT@', '-Wl,--whole-archive','@INPUT@', '-Wl,--no-whole-archive'],
        build_by_default: true
    )

    fs.copyfile('wacdfpp.html', 'wacdfpp.html')
endif
