project(
  'pycdfpp',
  'cpp',
  version : '0.2.3',
  default_options : ['warning_level=3', 'cpp_std=c++17'],
  license : 'GPL3'
)

if get_option('buildtype').contains('debug')
    add_project_arguments('-DCDFPP_ENABLE_ASSERT', language : ['cpp'])
    add_project_arguments('-DCDFPP_HEDLEY', language : ['cpp'])
endif


conf_data = configuration_data()
conf_data.set('version', meson.project_version())
if(target_machine.endian() == 'big')
    conf_data.set('CDFpp_BIG_ENDIAN', true)
else
    conf_data.set('CDFpp_LITTLE_ENDIAN', true)
endif
configure_file(input : 'config.h.in',
               output : 'config.h',
               format : 'cmake',
               install : true,
               install_dir : 'cdfpp',
               configuration : conf_data)

cpp = meson.get_compiler('cpp')
if('clang'==cpp.get_id())
    add_global_arguments('-fsized-deallocation', language : 'cpp')
endif


pybind11_dep = dependency('pybind11', required : true, fallback:['pybind11','pybind11_dep'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])

if build_machine.system() == 'windows'
    link_args = ['-static-libstdc++','-static-libgcc','-static']
    zlib_dep = meson.get_compiler('cpp').find_library('z', static: true, required:false)
    if not zlib_dep.found()
        zlib_dep = dependency('zlib', main : true, fallback : ['zlib', 'zlib_dep'])
    endif
else
    link_args = []
    zlib_dep = dependency('zlib', main : true, fallback : ['zlib', 'zlib_dep'])
endif




pymod = import('python')
python3 = pymod.find_installation('python3')

cdfpp_headers = [
    'include/attribute.hpp',
    'include/variable.hpp',
    'include/cdf.hpp',
    'include/cdf-helpers.hpp',
    'include/chrono/cdf-chrono.hpp',
    'include/chrono/cdf-chrono-constants.hpp',
    'include/chrono/cdf-leap-seconds.h',
    'include/cdf-data.hpp',
    'include/cdf-debug.hpp',
    'include/cdf-endianness.hpp',
    'include/cdf-majority-swap.hpp',
    'include/cdf-enums.hpp',
    'include/cdf-file.hpp',
    'include/cdf-io/cdf-io.hpp',
    'include/cdf-io/cdf-io-attribute.hpp',
    'include/cdf-io/cdf-io-buffers.hpp',
    'include/cdf-io/cdf-io-common.hpp',
    'include/cdf-io/cdf-io-desc-records.hpp',
    'include/cdf-io/cdf-io-variable.hpp',
    'include/cdf-io/cdf-io-zlib.hpp',
    'include/cdf-io/cdf-io-rle.hpp',
    'wrapper/repr.hpp',
    'wrapper/buffers.hpp'
]

cdfpp_extra_files = [
    'meson_options.txt',
    'pyproject.toml', 'README.md',
    'setup.py', 'tests/resources/make_cdf.py',
    'tests/python_wrapper/test.py',
    '.github/workflows/ccpp.yml',
    '.github/workflows/tests.yml',
    '.github/workflows/pythonpublish-linux.yml',
    '.github/workflows/pythonpublish-osx.yml',
    '.github/workflows/pythonpublish-win.yml'
]


cdfpp_lib = library('CDFpp',
        include_directories: include_directories('include'),
        dependencies: [zlib_dep, hedley_dep],
        extra_files: [cdfpp_headers, cdfpp_extra_files]
        )


cdfpp_dep = declare_dependency(include_directories: include_directories('include'),
                                dependencies: [zlib_dep, hedley_dep])



python3.extension_module('pycdfpp', 'wrapper/pycdfpp.cpp',
                         dependencies: [cdfpp_dep, pybind11_dep,python3.dependency()],
                         link_args: link_args,
                         install: true
                        )

install_headers(
[
    'include/attribute.hpp',
    'include/variable.hpp',
    'include/cdf.hpp',
    'include/cdf-helpers.hpp',
    'include/chrono/cdf-chrono.hpp',
    'include/chrono/cdf-chrono-constants.hpp',
    'include/chrono/cdf-leap-seconds.h',
    'include/cdf-data.hpp',
    'include/cdf-debug.hpp',
    'include/cdf-endianness.hpp',
    'include/cdf-majority-swap.hpp',
    'include/cdf-enums.hpp',
    'include/cdf-file.hpp',
    'include/cdf-io/cdf-io.hpp',
    'include/cdf-io/cdf-io-attribute.hpp',
    'include/cdf-io/cdf-io-buffers.hpp',
    'include/cdf-io/cdf-io-common.hpp',
    'include/cdf-io/cdf-io-desc-records.hpp',
    'include/cdf-io/cdf-io-variable.hpp',
    'include/cdf-io/cdf-io-zlib.hpp',
    'include/cdf-io/cdf-io-rle.hpp'
]
  , subdir : 'cdfpp')

pkg = import('pkgconfig')
pkg.generate(cdfpp_lib,
             subdirs : meson.project_name(),
             version : meson.project_version(),
             name : meson.project_name(),
             filebase : meson.project_name(),
             description : 'An header only modern C++ CDF library.')

if get_option('with_tests')

    configure_file(output : 'tests_config.hpp',
      configuration : {
        'DATA_PATH' : '"' + meson.current_source_dir() / 'tests/resources' + '"'
      }
    )

    catch_dep = dependency('catch2', main : true, fallback : ['catch2', 'catch2_dep'])


    foreach test:['endianness','simple_open', 'zlib', 'majority', 'chrono']
        exe = executable('test-'+test,'tests/'+test+'/main.cpp',
                        dependencies:[catch_dep, cdfpp_dep],
                        install: false
                        )
        test(test, exe)
    endforeach

    test('python_wrapper_test', python3,
        args:[meson.current_source_dir()+'/tests/python_wrapper/test.py'],
        env:['PYTHONPATH='+meson.current_build_dir()],
        workdir:meson.current_build_dir())

    python_wrapper_cpp = executable('python_wrapper_cpp','tests/python_wrapper_cpp/main.cpp',
                    dependencies:[pybind11_dep, python3.dependency(embed:true)],
                    install: false
                    )

    manual_load = executable('manual_load','tests/manual_load/main.cpp',
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
