
# Read by the top-level meson.build: what goes into the compiled cdfpp library.
cdfpp_no_simd = true
cdfpp_simd_sources = []
cdfpp_simd_libs = []
cdfpp_simd_args = []


if target_machine.cpu_family() == 'x86_64'
    x86_vectorized_libs = []
    x86_vectorized_defs = []
    xsimd_arch_list = []
    if cpp.get_id() == 'msvc'
        archs = [
            {
                'name':'avx512bw',
                'flags':['/arch:AVX512'],
                'xsimd_name':'avx512bw'
            },
            {
                'name':'avx2',
                'flags':['/arch:AVX2'],
                'xsimd_name':'avx2'
            },
            {
                'name':'sse2',
                'flags':['/arch:SSE2'],
                'xsimd_name':'sse2'
            }
            ]
    else
        archs = [
            {
                'name':'avx512bw',
                'flags':['-mavx512bw', '-mavx512cd', '-mavx512dq', '-mavx512vl'],
                'xsimd_name':'avx512bw'
            },
            {
                'name':'avx2',
                'flags':['-mavx2'],
                'xsimd_name':'avx2'
            },
            {
                'name':'sse2',
                'flags':['-msse2'],
                'xsimd_name':'sse2'
            }
            ]
    endif
    foreach arch : archs
        enable_arch_def = '-DCDFPP_ENABLE_'+arch['name'].to_upper()+'_ARCH'
        x86_vectorized_libs += [
            static_library('cdfpp_x86_vectorized_'+arch['name'],
                files('../src/arch/x86/chrono_arch.cpp'),
                include_directories : include_directories('../include', '..'),
                cpp_args : arch['flags'] + [enable_arch_def, '-DCDFPP_ARCH='+arch['xsimd_name']],
                dependencies : [xsimd_dep, hedley_dep, fmt_dep, cpp_utils_dep],
                )
        ]
        x86_vectorized_defs += [enable_arch_def]
        xsimd_arch_list += ['xsimd::'+arch['xsimd_name']]
    endforeach

    xsimd_arch_list = 'xsimd::arch_list<' + ', '.join(xsimd_arch_list) + '>'

    cdfpp_no_simd = false
    cdfpp_simd_sources = files('../src/arch/x86/chrono.cpp')
    cdfpp_simd_libs = x86_vectorized_libs
    cdfpp_simd_args = x86_vectorized_defs + ['-DCDFPP_XSIMD_ARCH_LIST=@0@'.format(xsimd_arch_list)]
endif
# Other architectures (aarch64, WebAssembly) use the scalar conversions: SIMD gains there are
# still to be investigated.
