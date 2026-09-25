#!/usr/bin/env python
# Sphinx configuration for the CDFpp / pycdfpp documentation.
import os

_here = os.path.dirname(os.path.abspath(__file__))

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.intersphinx',
    'sphinx.ext.autosectionlabel',
    'sphinx_design',
    'sphinx_copybutton',
    'nbsphinx',
    'numpydoc',
]

autosectionlabel_prefix_document = True
numpydoc_show_class_members = False
numpydoc_class_members_toctree = False
autodoc_member_order = 'bysource'

nbsphinx_execute = 'never'

nbsphinx_prolog = r"""
{% set docname = 'docs/' + env.doc2path(env.docname, base=None) %}

.. raw:: html

    <div class="admonition note">
      <p style="margin-bottom:0px">
        This page was generated from
        <a class="reference external" href="https://github.com/SciQLop/CDFpp/blob/main/{{ docname|e }}">{{ docname|e }}</a>.
        Run it online:
        <a href="https://mybinder.org/v2/gh/SciQLop/CDFpp/main/?labpath={{ docname|e }}"><img alt="Binder badge" src="https://mybinder.org/badge_logo.svg" style="vertical-align:text-bottom"></a>
        <a href="https://colab.research.google.com/github/SciQLop/CDFpp/blob/main/{{ docname|e }}"><img alt="Google Colab badge" src="https://colab.research.google.com/assets/colab-badge.svg" style="vertical-align:text-bottom"></a>
      </p>
    </div>
"""

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
exclude_patterns = ['_build', 'generated', 'Thumbs.db', '.DS_Store', 'superpowers', '**.ipynb_checkpoints']

project = 'CDFpp'
copyright = '2018-2026, Alexis Jeandet'
author = 'Alexis Jeandet'

with open(os.path.join(_here, '..', 'version.txt')) as f:
    version = f.read().strip()
release = version

language = 'en'

html_theme = 'furo'
html_title = f'CDFpp {version}'
html_static_path = ['_static']
html_css_files = ['playground.css']
html_js_files = ['playground.js']
html_theme_options = {
    'source_repository': 'https://github.com/SciQLop/CDFpp/',
    'source_branch': 'main',
    'source_directory': 'docs/',
}

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'numpy': ('https://numpy.org/doc/stable/', None),
    'matplotlib': ('https://matplotlib.org/stable/', None),
    'xarray': ('https://docs.xarray.dev/en/stable/', None),
    'pandas': ('https://pandas.pydata.org/docs/', None),
}
