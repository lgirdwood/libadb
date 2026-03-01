cmake_minimum_required(VERSION 3.10)
project(libastrodb VERSION 0.90
                   LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configure libtool version equivalent
set(LIBASTRODB_CURRENT_VERSION 0)
set(LIBASTRODB_RELEASE_VERSION 3)
set(LIBASTRODB_AGE_VERSION 0)
set(LIBASTRODB_VERSION 0.90)

# Default to release build
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Debug symbol support
option(ENABLE_DEBUG "enable debug symbols" OFF)
if(ENABLE_DEBUG)
  add_compile_definitions(HAVE_DEBUG=1)
  add_compile_options(-g)
else()
  add_compile_options(-O3)
endif()

# AVX support
option(ENABLE_AVX "enable AVX optimizations" OFF)
if(ENABLE_AVX)
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-mavx" COMPILER_SUPPORTS_AVX)
  if(COMPILER_SUPPORTS_AVX)
    add_compile_options(-mavx)
    add_compile_definitions(HAVE_AVX=1)
  else()
    message(FATAL_ERROR "Need a version of gcc with -mavx")
  endif()
endif()

# OpenMP support
option(ENABLE_OPENMP "enable OpenMP" OFF)
if(ENABLE_OPENMP)
  find_package(OpenMP)
  if(OpenMP_C_FOUND)
    add_compile_definitions(HAVE_OPENMP=1)
    set(OPENMP_CFLAGS ${OpenMP_C_FLAGS})
  else()
    message(FATAL_ERROR "Need a version of gcc with -fopenmp")
  endif()
endif()

# Dependencies
find_package(ZLIB REQUIRED)
find_library(M_LIB m REQUIRED)
find_library(FTP_LIB ftp REQUIRED)

# Core functions and header checks
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckTypeSize)

check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)

check_function_exists(bzero HAVE_BZERO)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(memmove HAVE_MEMMOVE)
check_function_exists(mkdir HAVE_MKDIR)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strstr HAVE_STRSTR)
check_function_exists(strtol HAVE_STRTOL)

# Other definitions
add_compile_definitions(_GNU_SOURCE)
add_compile_options(-Wall)

# Configuration File
configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)

enable_testing()
add_subdirectory(tests)

# Generate Doxygen documentation
find_package(Doxygen)
if (DOXYGEN_FOUND)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()

# Generate pkg-config file
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix \${prefix})
set(libdir \${exec_prefix}/lib)
set(includedir \${prefix}/include)
set(VERSION ${LIBASTRODB_VERSION})
configure_file(libastrodb.pc.in libastrodb.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libastrodb.pc
        DESTINATION lib/pkgconfig)

# Packaging support
set(CPACK_PACKAGE_NAME "libastrodb")
set(CPACK_PACKAGE_VERSION "${LIBASTRODB_VERSION}")
set(CPACK_PACKAGE_VENDOR "Liam Girdwood")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AstroDB library for astrometry and photometry")
set(CPACK_PACKAGE_CONTACT "Liam Girdwood")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Liam Girdwood")
include(CPack)

# Python bindings installation
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(pip-install
        COMMAND ${Python3_EXECUTABLE} -m pip install --break-system-packages -e ${CMAKE_CURRENT_SOURCE_DIR}/python
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Installing Python astrodb wrapper via pip"
        VERBATIM
    )
    
    # We can also add a Python test target
    add_custom_target(pytest
        COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python" ${Python3_EXECUTABLE} -m unittest discover -v -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running Python tests for astrodb wrapper"
        DEPENDS astrodb
        VERBATIM
    )
endif()
