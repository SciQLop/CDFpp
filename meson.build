project(
  'cdfpp',
  'cpp',
  version : '0.1.3',
  default_options : ['warning_level=3', 'cpp_std=c++17'],
  license : 'GPL3'
)

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

add_global_arguments('-DDATA_PATH="@0@/tests/resources"'.format(meson.current_source_dir()), language : 'cpp')

pybind11_dep = dependency('pybind11', required : true, fallback:['pybind11','pybind11_dep'])
zlib_dep = dependency('zlib', main : true, fallback : ['zlib', 'zlib_dep'])
python3_dep = dependency('python3')

cdfpp_lib = library('CDFpp',
        include_directories: include_directories('include'),
        dependencies: zlib_dep)

cdfpp_dep = declare_dependency(include_directories: include_directories('include'),
                                link_with:cdfpp_lib,
                                dependencies: zlib_dep)

pymod = import('python')
python3 = pymod.find_installation('python3')

python3.extension_module('pycdfpp', 'wrapper/pycdfpp.cpp',
                         dependencies: [cdfpp_dep, pybind11_dep],
                         install: true
                        )

install_headers(
['./include/attribute.hpp',
 './include/cdf-endianness.hpp',
 './include/cdf-enums.hpp',
 './include/variable.hpp',
 './include/cdf-data.hpp',
 './include/cdf-file.hpp',
 './include/cdf-io/cdf-io-buffers.hpp',
 './include/cdf-io/cdf-io-attribute.hpp',
 './include/cdf-io/cdf-io-zlib.hpp',
 './include/cdf-io/cdf-io-desc-records.hpp',
 './include/cdf-io/cdf-io-common.hpp',
 './include/cdf-io/cdf-io.hpp',
 './include/cdf-io/cdf-io-variable.hpp',
 './include/cdf.hpp']
  , subdir : 'cdfpp')

pkg = import('pkgconfig')
pkg.generate(cdfpp_lib,
             subdirs : meson.project_name(),
             version : meson.project_version(),
             name : meson.project_name(),
             filebase : meson.project_name(),
             description : 'An header only modern C++ CDF library.')

catch_dep = dependency('catch2', main : true, fallback : ['catch2', 'catch2_dep'])

#benchmark_dep = dependency('benchmark', fallback : ['google-benchmark', 'google_benchmark_dep'])

foreach test:['endianness','simple_open', 'zlib']
    exe = executable(test,'tests/'+test+'/main.cpp',
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
                dependencies:[cdfpp_dep, pybind11_dep, python3_dep],
                install: false
                )
