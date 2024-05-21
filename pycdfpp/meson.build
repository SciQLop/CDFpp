pycdfpp_python_sources = [files('__init__.py')]
fs = import('fs')
fs.copyfile('__init__.py', '__init__.py')

pymod = import('python')
python3 = pymod.find_installation('python3')

if meson.get_compiler('cpp').get_id() == 'msvc'
    pycdfpp_cpp_args = ['/bigobj']
else
    pycdfpp_cpp_args = []
endif


_pycdfpp = python3.extension_module('_pycdfpp', 'pycdfpp.cpp',
                         dependencies: [cdfpp_dep, fmt_dep, pybind11_dep,python3.dependency()],
                         link_args: link_args,
                         cpp_args: pycdfpp_cpp_args,
                         subdir: 'pycdfpp',
                         install: true,
                         extra_files: [pycdfpp_headers, cdfpp_headers, cdfpp_extra_files, cdfpp_doc_extra_files]
                        )

python3.install_sources(pycdfpp_python_sources, pure: false, subdir: 'pycdfpp')
