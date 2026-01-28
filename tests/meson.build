configure_file(output : 'tests_config.hpp',
  configuration : {
    'DATA_PATH' : '"' + meson.current_source_dir() / 'resources' + '"'
  }
)

catch_dep = dependency('catch2-with-main', version:'>3.0.0', required : true)


foreach test_name:['endianness','simple_open', 'majority', 'chrono', 'nomap', 'records_loading', 'records_saving',
              'rle_compression', 'libdeflate_compression', 'zlib_compression', 'simple_save', 'zstd_compression']
    exe = executable('test-'+test_name, test_name+'/main.cpp',
                    dependencies:[catch_dep, cdfpp_dep],
                    install: false
                    )
    test(test_name, exe)
endforeach

foreach py_test:['python_loading', 'python_saving', 'python_skeletons',
            'python_variable_set_values', 'full_corpus', 'python_chrono']
    test(py_test, python3,
        args:[files(py_test+'/test.py')],
        env:['PYTHONPATH='+meson.project_build_root()],
        timeout: 300,
        workdir:meson.current_build_dir())
endforeach


python_wrapper_cpp = executable('python_wrapper_cpp','python_wrapper_cpp/main.cpp',
                dependencies:[pybind11_dep, python3.dependency(embed:true), catch_dep, cdfpp_dep, fmt_dep],
                install: false
                )

test('python_wrapper_cpp', python_wrapper_cpp,
    env:['PYTHONPATH='+meson.project_build_root()],
    timeout: 300,
    workdir:meson.current_build_dir())

manual_load = executable('manual_load','manual_load/main.cpp',
                dependencies:[cdfpp_dep],
                install: false
                )

manual_load = executable('manual_save','manual_save/main.cpp',
                dependencies:[cdfpp_dep],
                install: false
                )


foreach example:['basic_cpp']
    exe = executable('example-'+example,'../examples/'+example+'/main.cpp',
                    dependencies:[cdfpp_dep],
                    cpp_args: ['-DDATA_PATH="@0@/resources"'.format(meson.current_source_dir())],
                    install: false
                    )
endforeach
