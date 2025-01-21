#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# speasy documentation build configuration file, created by
# sphinx-quickstart on Fri Jun  9 13:47:02 2017.
#
# This file is execfile()d with the current directory set to its
# containing dir.
#
# Note that not all possible configuration values are present in this
# autogenerated file.
#
# All configuration values have a default; values that are commented out
# serve to show the default.

# If extensions (or modules to document with autodoc) are in another
# directory, add these directories to sys.path here. If the directory is
# relative to the documentation root, use os.path.abspath to make it
# absolute, like shown here.
#

# -- General configuration ---------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
# needs_sphinx = '1.0'
import os
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom ones.
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.doctest',
    'sphinx.ext.intersphinx',
    'sphinx.ext.autosectionlabel',
    'sphinx_gallery.load_style',
    'sphinx_codeautolink',
    'sphinxcontrib.apidoc',
    'nbsphinx',
    'numpydoc']

apidoc_module_dir = os.environ.get("PYCDFPP_PATH",'..')+'/pycdfpp'
apidoc_output_dir = 'generated/'
apidoc_separate_modules = True
apidoc_module_first = True

autosectionlabel_prefix_document = True
codeautolink_custom_blocks = {
    "python3": None,
    "pycon3": "sphinx_codeautolink.clean_pycon",
}

nbsphinx_execute_arguments = [
    "--InlineBackend.figure_formats={'svg', 'pdf'}",
    "--InlineBackend.rc=figure.dpi=96",
]

nbsphinx_execute = 'never'

nbsphinx_thumbnails = {
    "examples/CDF_loading": "_static/CDF_loading_nb_thumbnail.png",
}

nbsphinx_prolog = r"""
{% set docname = 'docs/' + env.doc2path(env.docname, base=None) %}
{% set nb_base = 'tree' if env.config.revision else 'blob' %}
{% set nb_where = env.config.revision if env.config.revision else 'main' %}

.. raw:: html

    <div class="admonition note">
      <p style="margin-bottom:0px">
        This page was generated by
        <a href="https://nbsphinx.readthedocs.io/">nbsphinx</a> from
        <a class="reference external" href="https://github.com/SciQLop/cdfpp/{{ nb_base|e }}/{{ nb_where|e }}/{{ docname|e }}">{{ docname|e }}</a>.
        <br>
        Interactive online version:
        <a href="https://mybinder.org/v2/gh/SciQLop/cdfpp/{{ nb_where|e }}/?labpath={{ docname|e }}"><img alt="Binder badge" src="https://mybinder.org/badge_logo.svg" style="vertical-align:text-bottom"></a>
        <a href="https://colab.research.google.com/github/SciQLop/cdfpp/blob/{{ nb_where|e }}/{{ docname|e }}"><img alt="Google Colab badge" src="https://colab.research.google.com/assets/colab-badge.svg" style="vertical-align:text-bottom"></a>
      </p>
    </div>

.. raw:: latex

    \nbsphinxstartnotebook{\scriptsize\noindent\strut
    \textcolor{gray}{The following section was generated from
    \sphinxcode{\sphinxupquote{\strut {{ docname | escape_latex }}}} \dotfill}}
"""

mathjax3_config = {
    'tex': {'tags': 'ams', 'useLabelIds': True},
}


# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

# General information about the project.
project = u'PyCDFpp'
copyright = u"2018-2023, Alexis Jeandet"
author = u"Alexis Jeandet"

# The version info for the project you're documenting, acts as replacement
# for |version| and |release|, also used in various other places throughout
# the built documents.
#
# The short X.Y version.
version = '0.7.5'
# The full version, including alpha/beta/rc tags.
release = version

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = 'en'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This patterns also effect to html_static_path and html_extra_path
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

numpydoc_show_class_members = False

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = False

# -- Options for HTML output -------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Theme options are theme-specific and customize the look and feel of a
# theme further.  For a list of options available for each theme, see the
# documentation.
#
# html_theme_options = {}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]

# -- Options for HTMLHelp output ---------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = 'PyCDFppdoc'

# -- Options for LaTeX output ------------------------------------------

latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    #
    # 'papersize': 'letterpaper',

    # The font size ('10pt', '11pt' or '12pt').
    #
    # 'pointsize': '10pt',

    # Additional stuff for the LaTeX preamble.
    #
    # 'preamble': '',

    # Latex figure (float) alignment
    #
    # 'figure_align': 'htbp',
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title, author, documentclass
# [howto, manual, or own class]).
latex_documents = [
    (master_doc, 'pcdfpp.tex',
     u'PyCDFpp Documentation',
     u'Alexis Jeandet', 'manual'),
]

# -- Options for manual page output ------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (master_doc, 'PyCDFpp',
     u'PyCDFpp Documentation',
     [author], 1)
]

# -- Options for Texinfo output ----------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    (master_doc, 'PyCDFpp',
     u'PyCDFpp Documentation',
     author,
     'PyCDFpp',
     'One line description of project.',
     'Miscellaneous'),
]


intersphinx_mapping = {'python3': ('https://docs.python.org/3', None),
                       'numpy': ('https://numpy.org/doc/stable/', None),
                       'scipy': ('https://docs.scipy.org/doc/scipy/', None),
                       'matplotlib': ('https://matplotlib.org/stable/', None),
                       'seaborn': ('https://seaborn.pydata.org/', None),
                       'astropy': ('https://docs.astropy.org/en/stable/', None),
                       'requests': ('https://requests.readthedocs.io/en/latest/', None),
                       'xarray': ('https://xarray.pydata.org/en/stable/', None),
                       }
