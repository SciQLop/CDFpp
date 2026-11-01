cdflib_xarray_python_sources = files('__init__.py', 'cdf_to_xarray.py')
fs.copyfile('__init__.py', '__init__.py')
fs.copyfile('cdf_to_xarray.py', 'cdf_to_xarray.py')

python3.install_sources(cdflib_xarray_python_sources, pure: false, subdir: 'pycdfpp/cdflib/xarray')
