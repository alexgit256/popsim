from setuptools import setup, Extension
from Cython.Build import cythonize
import numpy

ext = Extension(
    "popsim.popsim",
    sources=[
        "src/popsim/popsim.pyx",       # Cython
        "src/popsim/population.cpp",   # your C++ core
    ],
    include_dirs=[
        "src/popsim",
        numpy.get_include(),
    ],
    language="c++",
    extra_compile_args=["-O3", "-std=c++17"],
)

setup(
    name="popsim",
    version="0.1.0",
    package_dir={"": "src"},
    packages=["popsim"],
    include_package_data=True,  # works with MANIFEST.in
    ext_modules=cythonize([ext], language_level=3),
)


