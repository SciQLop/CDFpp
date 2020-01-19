from skbuild import setup

with open('README.md') as readme_file:
    readme = readme_file.read()

setup(
    name="pycdfpp",
    version="0.1.3",
    description="A modern C++ header only cdf library",
    author='Alexis Jeandet',
    author_email='alexis.jeandet@member.fsf.org',
        classifiers=[
            'Development Status :: 2 - Pre-Alpha',
            'Intended Audience :: Developers',
            'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
            'Natural Language :: English',
            'Programming Language :: Python :: 3',
            'Programming Language :: Python :: 3.5',
            'Programming Language :: Python :: 3.6',
            'Programming Language :: Python :: 3.7',
            'Programming Language :: Python :: 3.8',
        ],
    license="GNU General Public License v3",
    long_description=readme,
    long_description_content_type='text/markdown',
    keywords='CDF NASA Space Physics Plasma',
    url='https://github.com/SciQLop/CDFpp',
    packages=['pycdfpp']
)
