cdflib_python_sources = files('__init__.py', '_records.py', '_types.py', 'cdfread.py', 'dataclasses.py', 'epochs.py',
    'epochs_astropy.py',
    's3.py', 'utils.py', 'logging.py')
fs.copyfile('__init__.py', '__init__.py')
fs.copyfile('_records.py', '_records.py')
fs.copyfile('_types.py', '_types.py')
fs.copyfile('cdfread.py', 'cdfread.py')
fs.copyfile('dataclasses.py', 'dataclasses.py')
fs.copyfile('epochs.py', 'epochs.py')
fs.copyfile('epochs_astropy.py', 'epochs_astropy.py')
fs.copyfile('s3.py', 's3.py')
fs.copyfile('utils.py', 'utils.py')
fs.copyfile('logging.py', 'logging.py')

python3.install_sources(cdflib_python_sources, pure: false, subdir: 'pycdfpp/cdflib')
