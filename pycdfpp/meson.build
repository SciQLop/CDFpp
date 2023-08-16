pycdfpp_python_sources = [files('__init__.py')]
configure_file(input:'__init__.py',output:'__init__.py', copy:true)

pymod = import('python')
python3 = pymod.find_installation('python3')


python3.extension_module('_pycdfpp', 'pycdfpp.cpp',
                         dependencies: [cdfpp_dep, fmt_dep, pybind11_dep,python3.dependency()],
                         link_args: link_args,
                         subdir : 'pycdfpp',
                         install: true,
                         extra_files:[pycdfpp_headers, cdfpp_headers, cdfpp_extra_files]
                        )

python3.install_sources(pycdfpp_python_sources, pure: false, subdir: 'pycdfpp')
