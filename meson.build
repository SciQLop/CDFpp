project(
  'cdfpp',
  'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'],
  license : 'GPL3'
)

# TODO add a config file
if(target_machine.endian() == 'big')
    add_global_arguments('-DCDFpp_BIG_ENDIAN', language : 'cpp')
else
    add_global_arguments('-DCDFpp_LITTLE_ENDIAN', language : 'cpp')
endif

add_global_arguments('-DDATA_PATH="@0@/tests/resources"'.format(meson.current_source_dir()), language : 'cpp')


zlib_dep = dependency('zlib', main : true, fallback : ['zlib', 'zlib_dep'])

cdfpp_lib = library('CDFpp',
        include_directories: include_directories('include'),
        dependencies: zlib_dep)

cdfpp_dep = declare_dependency(include_directories: include_directories('include'),
                                link_with:cdfpp_lib,
                                dependencies: zlib_dep)


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

