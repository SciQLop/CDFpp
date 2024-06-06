project(
  'pycdfpp',
  'cpp','c',
  meson_version: '>= 0.64.0',
  version : run_command('scripts/version.py',
                        capture:true,
                        env:{'SRC_ROOT':meson.project_source_root()}
                        ).stdout().strip() ,
  default_options : ['warning_level=3', 'cpp_std=c++17', 'default_library=static', 'buildtype=release'],
  license : 'GPL3'
)

if get_option('buildtype').contains('debug')
    add_project_arguments('-DCDFPP_ENABLE_ASSERT', language : ['cpp'])
    add_project_arguments('-DCDFPP_HEDLEY', language : ['cpp'])
endif


conf_data = configuration_data()
conf_data.set_quoted('CDFPP_VERSION', meson.project_version())

if get_option('use_libdeflate')
    conf_data.set('CDFpp_USE_LIBDEFLATE', true)
endif

if get_option('use_nomap')
    conf_data.set('CDFpp_USE_NOMAP', true)
endif

if(target_machine.endian() == 'big')
    conf_data.set('CDFpp_BIG_ENDIAN', true)
    conf_data.set('CDFpp_ENCODING', 'cdf_encoding::IBMRS')
else
    conf_data.set('CDFpp_LITTLE_ENDIAN', true)
    conf_data.set('CDFpp_ENCODING', 'cdf_encoding::IBMPC')
endif
configure_file(output : 'cdfpp_config.h',
               install : true,
               install_dir : 'include/cdfpp',
               configuration : conf_data)

cpp = meson.get_compiler('cpp')
if('clang'==cpp.get_id())
    add_global_arguments('-fsized-deallocation', language : 'cpp')
endif


pybind11_dep = dependency('pybind11')
hedley_dep = dependency('hedley')
fmt_dep = dependency('fmt')

if build_machine.system() == 'windows'
    link_args = ['-static-libstdc++','-static-libgcc','-static']
elif build_machine.system() == 'darwin' and cpp.get_id() != 'clang'
    link_args = ['-static-libstdc++','-static-libgcc']
else
    link_args = []
endif

if get_option('use_libdeflate')
    zlib_dep = dependency('libdeflate')
else
    if build_machine.system() == 'windows'
        zlib_dep = meson.get_compiler('cpp').find_library('z', static: true, required:false)
        if not zlib_dep.found()
            zlib_dep = dependency('zlib', main : true, static: true)
        endif
    else
        zlib_dep = dependency('zlib', main : true, fallback : ['zlib', 'zlib_dep'])
    endif
endif


cdfpp_headers = files(
    'include/cdfpp/attribute.hpp',
    'include/cdfpp/nomap.hpp',
    'include/cdfpp/no_init_vector.hpp',
    'include/cdfpp/cdf-map.hpp',
    'include/cdfpp/variable.hpp',
    'include/cdfpp/cdf.hpp',
    'include/cdfpp/cdf-helpers.hpp',
    'include/cdfpp/cdf-repr.hpp',
    'include/cdfpp/cdf-data.hpp',
    'include/cdfpp/cdf-debug.hpp',
    'include/cdfpp/cdf-enums.hpp',
    'include/cdfpp/cdf-file.hpp',
    'include/cdfpp/chrono/cdf-chrono.hpp',
    'include/cdfpp/chrono/cdf-chrono-constants.hpp',
    'include/cdfpp/chrono/cdf-leap-seconds.h',
    'include/cdfpp/cdf-io/cdf-io.hpp',
    'include/cdfpp/cdf-io/common.hpp',
    'include/cdfpp/cdf-io/reflection.hpp',
    'include/cdfpp/cdf-io/desc-records.hpp',
    'include/cdfpp/cdf-io/endianness.hpp',
    'include/cdfpp/cdf-io/majority-swap.hpp',
    'include/cdfpp/cdf-io/special-fields.hpp',
    'include/cdfpp/cdf-io/decompression.hpp',
    'include/cdfpp/cdf-io/compression.hpp',
    'include/cdfpp/cdf-io/zlib.hpp',
    'include/cdfpp/cdf-io/rle.hpp',
    'include/cdfpp/cdf-io/libdeflate.hpp',
    'include/cdfpp/cdf-io/loading/loading.hpp',
    'include/cdfpp/cdf-io/loading/records-loading.hpp',
    'include/cdfpp/cdf-io/loading/attribute.hpp',
    'include/cdfpp/cdf-io/loading/buffers.hpp',
    'include/cdfpp/cdf-io/loading/variable.hpp',
    'include/cdfpp/cdf-io/saving/saving.hpp',
    'include/cdfpp/cdf-io/saving/records-saving.hpp',
    'include/cdfpp/cdf-io/saving/buffers.hpp',
    'include/cdfpp/cdf-io/saving/create_records.hpp',
    'include/cdfpp/cdf-io/saving/layout_records.hpp',
    'include/cdfpp/cdf-io/saving/link_records.hpp'
)

if get_option('with_experimental_zstd')
    add_project_arguments('-DCDFPP_USE_ZSTD', language : ['cpp'])
    zstd_dep = dependency('libzstd')
    cdfpp_headers += files(
        'include/cdfpp/cdf-io/zstd.hpp'
    )
else
    zstd_dep = declare_dependency()
endif


pycdfpp_headers = files(
    'pycdfpp/chrono.hpp',
    'pycdfpp/buffers.hpp',
    'pycdfpp/cdf.hpp',
    'pycdfpp/attribute.hpp',
    'pycdfpp/variable.hpp',
    'pycdfpp/enums.hpp',
    'pycdfpp/repr.hpp'
)

cdfpp_extra_files = files(
    'pyproject.toml',
    'README.md',
    '.bumpversion.cfg',
    'version.txt',
    'scripts/build_wheel.py',
    'scripts/CDF.hexpat',
    'tests/resources/make_cdf.py',
    'tests/python_loading/test.py',
    'tests/python_saving/test.py',
    'tests/python_variable_set_values/test.py',
    'tests/python_skeletons/test.py',
    'tests/full_corpus/test_full_corpus.py',
    '.github/workflows/CI.yml',
    '.github/workflows/tests-with-coverage.yml'
)

cdfpp_doc_extra_files =  files(
'docs/conf.py',
'docs/Makefile',
'docs/requirements.txt',
'docs/index.rst',
'docs/authors.rst',
'docs/contributing.rst',
'docs/history.rst',
'docs/installation.rst',
'docs/examples/index.rst',
'docs/examples/Loading_CDF.ipynb',
'docs/examples/Creating_CDF.ipynb'
)

cdfpp_dep_inc = include_directories('include', '.')


cdfpp_dep = declare_dependency(include_directories: cdfpp_dep_inc,
                                dependencies: [zlib_dep, hedley_dep, fmt_dep, zstd_dep])

if get_option('disable_python_wrapper')
    message('building without Python wrapper')
else
    subdir('pycdfpp')
    subdir('docs')
endif



install_headers(
[
    'include/cdfpp/attribute.hpp',
    'include/cdfpp/nomap.hpp',
    'include/cdfpp/no_init_vector.hpp',
    'include/cdfpp/cdf-map.hpp',
    'include/cdfpp/variable.hpp',
    'include/cdfpp/cdf.hpp',
    'include/cdfpp/cdf-helpers.hpp',
    'include/cdfpp/cdf-repr.hpp',
    'include/cdfpp/cdf-data.hpp',
    'include/cdfpp/cdf-debug.hpp',
    'include/cdfpp/cdf-enums.hpp',
    'include/cdfpp/cdf-file.hpp'
], subdir:'cdfpp')

install_headers(
[
    'include/cdfpp/chrono/cdf-chrono.hpp',
    'include/cdfpp/chrono/cdf-chrono-constants.hpp',
    'include/cdfpp/chrono/cdf-leap-seconds.h',
], subdir:'cdfpp/chrono')

install_headers(
[
    'include/cdfpp/cdf-io/cdf-io.hpp',
    'include/cdfpp/cdf-io/reflection.hpp',
    'include/cdfpp/cdf-io/special-fields.hpp',
    'include/cdfpp/cdf-io/common.hpp',
    'include/cdfpp/cdf-io/desc-records.hpp',
    'include/cdfpp/cdf-io/decompression.hpp',
    'include/cdfpp/cdf-io/compression.hpp',
    'include/cdfpp/cdf-io/zlib.hpp',
    'include/cdfpp/cdf-io/libdeflate.hpp',
    'include/cdfpp/cdf-io/rle.hpp',
    'include/cdfpp/cdf-io/majority-swap.hpp',
    'include/cdfpp/cdf-io/endianness.hpp'
], subdir:'cdfpp/cdf-io')

install_headers(
[
    'include/cdfpp/cdf-io/loading/attribute.hpp',
    'include/cdfpp/cdf-io/loading/buffers.hpp',
    'include/cdfpp/cdf-io/loading/loading.hpp',
    'include/cdfpp/cdf-io/loading/records-loading.hpp',
    'include/cdfpp/cdf-io/loading/variable.hpp',
], subdir:'cdfpp/cdf-io/loading')

install_headers(
[
    'include/cdfpp/cdf-io/saving/saving.hpp',
    'include/cdfpp/cdf-io/saving/records-saving.hpp',
    'include/cdfpp/cdf-io/saving/buffers.hpp',
    'include/cdfpp/cdf-io/saving/create_records.hpp',
    'include/cdfpp/cdf-io/saving/layout_records.hpp',
    'include/cdfpp/cdf-io/saving/link_records.hpp'
], subdir:'cdfpp/cdf-io/saving')


subdir('wacdfpp')


if get_option('with_tests')

    configure_file(output : 'tests_config.hpp',
      configuration : {
        'DATA_PATH' : '"' + meson.current_source_dir() / 'tests/resources' + '"'
      }
    )

    catch_dep = dependency('catch2-with-main', version:'>3.0.0', required : true)


    foreach test:['endianness','simple_open', 'majority', 'chrono', 'nomap', 'records_loading', 'records_saving',
                  'rle_compression', 'libdeflate_compression', 'zlib_compression', 'simple_save', 'zstd_compression']
        exe = executable('test-'+test,'tests/'+test+'/main.cpp',
                        dependencies:[catch_dep, cdfpp_dep],
                        install: false
                        )
        test(test, exe)
    endforeach

    test('python_loading_test', python3,
        args:[meson.current_source_dir()+'/tests/python_loading/test.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        workdir:meson.current_build_dir())

    test('python_saving_test', python3,
        args:[meson.current_source_dir()+'/tests/python_saving/test.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        workdir:meson.current_build_dir())

    test('python_skeletons_test', python3,
        args:[meson.current_source_dir()+'/tests/python_skeletons/test.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        workdir:meson.current_build_dir())

    test('python_variable_set_values_test', python3,
        args:[meson.current_source_dir()+'/tests/python_variable_set_values/test.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        workdir:meson.current_build_dir())

    test('full_corpus_test', python3,
        args:[meson.current_source_dir()+'/tests/full_corpus/test_full_corpus.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        timeout: 300,
        workdir:meson.current_build_dir())

    python_wrapper_cpp = executable('python_wrapper_cpp','tests/python_wrapper_cpp/main.cpp',
                    dependencies:[pybind11_dep, python3.dependency(embed:true)],
                    install: false
                    )

    manual_load = executable('manual_load','tests/manual_load/main.cpp',
                    dependencies:[cdfpp_dep],
                    install: false
                    )

    manual_load = executable('manual_save','tests/manual_save/main.cpp',
                    dependencies:[cdfpp_dep],
                    install: false
                    )


    foreach example:['basic_cpp']
        exe = executable('example-'+example,'examples/'+example+'/main.cpp',
                        dependencies:[cdfpp_dep],
                        cpp_args: ['-DDATA_PATH="@0@/tests/resources"'.format(meson.current_source_dir())],
                        install: false
                        )
    endforeach
endif

if get_option('with_benchmarks')
    google_benchmarks_dep = dependency('benchmark', required : true)
    foreach bench:['file_reader']
        exe = executable('benchmark-'+bench,'benchmarks/'+bench+'/main.cpp',
                        dependencies:[google_benchmarks_dep, cdfpp_dep],
                        install: false
                        )
        benchmark(bench, exe)
    endforeach
endif

summary({'C': meson.get_compiler('c').cmd_array(),
        'C++': meson.get_compiler('cpp').cmd_array()
        }, section: 'Compilers')
