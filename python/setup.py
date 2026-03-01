import os
from setuptools import setup, find_packages

# Read the version from CMakeLists.txt or similar, or just hardcode for simplicity
VERSION = "0.90"

setup(
    name="astrodb",
    version=VERSION,
    description="Python wrapper for the libastrodb astrometry/photometry C API",
    packages=find_packages(),
    author="Liam Girdwood",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU Lesser General Public License v2 (LGPLv2)",
        "Operating System :: POSIX :: Linux",
    ],
    python_requires=">=3.6",
)
