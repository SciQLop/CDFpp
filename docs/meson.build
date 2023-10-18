custom_target('docs',
                output: 'doc.txt',
                command: [python3, '-msphinx', meson.current_source_dir(), meson.current_build_dir() ], build_by_default: false,
                depends: _pycdfpp,
                depend_files: cdfpp_doc_extra_files,
                env: {'SPHINXPROJ': 'PyCDFpp', 'PYTHONPATH':meson.project_build_root(), 'PYCDFPP_PATH':meson.project_build_root()})
