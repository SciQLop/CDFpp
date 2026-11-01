cdflib_xarray_python_sources = files('__init__.py', '_istp.py', 'cdf_to_xarray.py', 'xarray_to_cdf.py')
fs.copyfile('__init__.py', '__init__.py')
fs.copyfile('cdf_to_xarray.py', 'cdf_to_xarray.py')
fs.copyfile('_istp.py', '_istp.py')
fs.copyfile('xarray_to_cdf.py', 'xarray_to_cdf.py')

python3.install_sources(cdflib_xarray_python_sources, pure: false, subdir: 'pycdfpp/cdflib/xarray')
