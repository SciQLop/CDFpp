# Native meson port of c-blosc2's library target (tests, benches, plugins and zlib left out).
# Meson's cmake module cannot apply per-source compile flags: it spreads bitshuffle-avx512.c's
# -mavx512f/-mavx512bw over the whole library, which crashes with SIGILL on CPUs without
# AVX512. Each SIMD kernel therefore lives in its own static library with its own flags.
project('c-blosc2', 'c',
    version : '3.3.4',
    license : 'BSD-3-Clause',
    meson_version : '>= 0.64.0',
    default_options : ['warning_level=0', 'default_library=static'])

cc = meson.get_compiler('c')

deps = [
    dependency('liblz4'),
    dependency('libzstd', default_options : ['multi_thread=disabled', 'bin_programs=false', 'zlib=disabled', 'lzma=disabled', 'lz4=disabled']),   # see CDFpp's meson.build
    # Emscripten's -pthread would turn the whole wasm link into a shared-memory build; its
    # default libc already stubs the pthread API, which is all blosc2 needs with nthreads=1.
    cc.get_id() == 'emscripten' ? dependency('', required : false) : dependency('threads'),
    cc.find_library('m', required : false),
    cc.find_library('rt', required : false),
    cc.find_library('dl', required : false),
]

conf = configuration_data()
conf.set('HAVE_ZSTD', 1)
configure_file(input : 'blosc/config.h.in', output : 'config.h', format : 'cmake@',
    configuration : conf)

inc = include_directories('include', 'blosc', '.')
c_args = ['-DUSING_CMAKE', '-DBUILD_STATIC']

sources = files(
    'blosc/blosc2.c', 'blosc/blosclz.c', 'blosc/fastcopy.c', 'blosc/schunk.c', 'blosc/frame.c',
    'blosc/stune.c', 'blosc/delta.c', 'blosc/shuffle-generic.c', 'blosc/bitshuffle-generic.c',
    'blosc/trunc-prec.c', 'blosc/timestamp.c', 'blosc/sframe.c', 'blosc/directories.c',
    'blosc/blosc2-stdio.c', 'blosc/b2nd.c', 'blosc/b2nd_utils.c',
)
if host_machine.system() == 'windows'
    sources += files('blosc/win32/threading.c')
endif

# [sources, flags, dispatch define] per SIMD kernel, compiled only when the compiler accepts
# the flags; shuffle.c picks the implementation at runtime from the defines.
cpu = host_machine.cpu_family()
msvc = cc.get_argument_syntax() == 'msvc'
kernels = []
if cpu in ['x86_64', 'x86']
    kernels += [
        [['blosc/shuffle-sse2.c', 'blosc/bitshuffle-sse2.c'], msvc ? [] : ['-msse2'], 'SHUFFLE_SSE2_ENABLED'],
        [['blosc/shuffle-avx2.c', 'blosc/bitshuffle-avx2.c'], msvc ? ['/arch:AVX2'] : ['-mavx2'],
            'SHUFFLE_AVX2_ENABLED'],
        [['blosc/bitshuffle-avx512.c'], msvc ? ['/arch:AVX512'] : ['-mavx512f', '-mavx512bw'],
            'SHUFFLE_AVX512_ENABLED'],
    ]
elif cpu == 'aarch64'
    kernels += [
        [['blosc/shuffle-neon.c'], ['-flax-vector-conversions'], 'SHUFFLE_NEON_ENABLED'],
    ]
endif

kernel_libs = []
dispatch_args = []
foreach kernel : kernels
    if cc.has_multi_arguments(kernel[1])
        kernel_libs += static_library('blosc2-' + kernel[2].to_lower(), kernel[0],
            include_directories : inc,
            c_args : c_args + kernel[1],
            dependencies : deps,
            pic : true)
        dispatch_args += '-D' + kernel[2]
    endif
endforeach

kernel_libs += static_library('blosc2-dispatch', 'blosc/shuffle.c',
    include_directories : inc,
    c_args : c_args + dispatch_args,
    dependencies : deps,
    pic : true)

blosc2_lib = static_library('blosc2', sources,
    include_directories : inc,
    c_args : c_args,
    link_whole : kernel_libs,
    dependencies : deps,
    gnu_symbol_visibility : 'hidden',
    pic : true)

blosc2_dep = declare_dependency(
    include_directories : include_directories('include'),
    link_with : blosc2_lib,
    dependencies : deps)
meson.override_dependency('blosc2', blosc2_dep)
