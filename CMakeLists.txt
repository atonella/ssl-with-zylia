cmake_minimum_required(VERSION 3.15)
project(array2sh_poc LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ALSA
find_package(ALSA REQUIRED)

# SAF root directory (parent of poc folder)
set(SAF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Define which BLAS/LAPACK to use (required by SAF)
# Options: SAF_USE_INTEL_MKL, SAF_USE_OPEN_BLAS_AND_LAPACKE, SAF_USE_APPLE_ACCELERATE, etc.
add_definitions(-DSAF_USE_OPEN_BLAS_AND_LAPACKE)

# SAF include directories
set(SAF_INCLUDE_DIRS
    ${SAF_ROOT}/framework/include
    ${SAF_ROOT}/framework/modules/saf_utilities
    ${SAF_ROOT}/framework/modules/saf_hoa
    ${SAF_ROOT}/framework/modules/saf_sh
    ${SAF_ROOT}/framework/modules/saf_vbap
    ${SAF_ROOT}/framework/modules/saf_cdf4sap
    ${SAF_ROOT}/framework/modules/saf_hrir
    ${SAF_ROOT}/framework/modules/saf_reverb
    ${SAF_ROOT}/framework/modules/saf_tracker
    ${SAF_ROOT}/framework/modules/saf_sofa_reader
    ${SAF_ROOT}/examples/include
)

# SAF library path (check where libsaf.a or libsaf.so is located)
set(SAF_LIBRARY_DIR "${SAF_ROOT}/build/framework")
set(SAF_EXAMPLES_LIBRARY_DIR "${SAF_ROOT}/build/examples")

# Find SAF library
find_library(SAF_LIBRARY 
    NAMES saf
    PATHS ${SAF_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

# Find SAF example libraries
find_library(SAF_EXAMPLE_ARRAY2SH_LIBRARY 
    NAMES saf_example_array2sh
    PATHS ${SAF_EXAMPLES_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

find_library(SAF_EXAMPLE_SLDOA_LIBRARY 
    NAMES saf_example_sldoa
    PATHS ${SAF_EXAMPLES_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

if(NOT SAF_LIBRARY)
    message(FATAL_ERROR "SAF library not found in ${SAF_LIBRARY_DIR}. Please build SAF first:\n  cd ${SAF_ROOT} && mkdir -p build && cd build && cmake .. && make")
endif()

if(NOT SAF_EXAMPLE_ARRAY2SH_LIBRARY)
    message(FATAL_ERROR "SAF example array2sh library not found in ${SAF_EXAMPLES_LIBRARY_DIR}")
endif()

if(NOT SAF_EXAMPLE_SLDOA_LIBRARY)
    message(FATAL_ERROR "SAF example sldoa library not found in ${SAF_EXAMPLES_LIBRARY_DIR}")
endif()

message(STATUS "Found SAF library: ${SAF_LIBRARY}")
message(STATUS "Found SAF example array2sh library: ${SAF_EXAMPLE_ARRAY2SH_LIBRARY}")
message(STATUS "Found SAF example sldoa library: ${SAF_EXAMPLE_SLDOA_LIBRARY}")

# Create executable
add_executable(array2sh_poc array2sh.cpp)

# Include directories
target_include_directories(array2sh_poc PRIVATE
    ${SAF_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${OPENBLAS_INCLUDE_DIRS}
    ${LAPACKE_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
)

# Find OpenBLAS and LAPACKE
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENBLAS openblas)
pkg_check_modules(LAPACKE lapacke)
pkg_check_modules(FFTW3F fftw3f)

# Link libraries
target_link_libraries(array2sh_poc PRIVATE
    ${SAF_EXAMPLE_ARRAY2SH_LIBRARY}
    ${SAF_EXAMPLE_SLDOA_LIBRARY}
    ${SAF_LIBRARY}
    ${ALSA_LIBRARIES}
    ${OPENBLAS_LIBRARIES}
    ${LAPACKE_LIBRARIES}
    m
    pthread
)