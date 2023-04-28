#*******************************************************************************
# Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
# Notified per clause 4(b) of the license.
#******************************************************************************/

#===============================================================================
# Copyright 2018-2022 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#===============================================================================

# Manage different library options
#===============================================================================

if(options_cmake_included)
    return()
endif()
set(options_cmake_included true)

# ========
# Features
# ========

option(ZENDNN_VERBOSE
    "allows ZENDNN be verbose whenever ZENDNN_VERBOSE
    environment variable set to 1" OFF) # disabled by default

option(ZENDNN_ENABLE_CONCURRENT_EXEC
    "disables sharing a common scratchpad between primitives.
    This option must be turned ON if there is a possibility of executing
    distinct primitives concurrently.
    CAUTION: enabling this option increases memory consumption."
    OFF) # disabled by default

option(ZENDNN_ENABLE_PRIMITIVE_CACHE "enables primitive cache." ON)
    # enabled by default

option(ZENDNN_USE_RT_OBJECTS_IN_PRIMITIVE_CACHE "If ZENDNN_ENABLE_PRIMITIVE_CACHE
    is ON enables using runtime objects in the primitive cache" ON)

option(ZENDNN_ENABLE_MAX_CPU_ISA
    "enables control of CPU ISA detected by ZENDNN via ZENDNN_MAX_CPU_ISA
    environment variable and zendnn_set_max_cpu_isa() function" OFF)

option(ZENDNN_ENABLE_CPU_ISA_HINTS
    "enables control of CPU ISA specific hints by ZENDNN via ZENDNN_CPU_ISA_HINTS
    environment variable and zendnn_set_cpu_isa_hints() function" OFF)

# =============================
# Building properties and scope
# =============================

set(ZENDNN_LIBRARY_TYPE "SHARED" CACHE STRING
    "specifies whether ZENDNN library should be SHARED or STATIC")
option(ZENDNN_BUILD_EXAMPLES "builds examples"  OFF)
option(ZENDNN_BUILD_TESTS "builds tests" ON)
option(ZENDNN_BUILD_FOR_CI
    "specifies whether ZENDNN library will use special testing environment for
    internal testing processes"
    OFF)
option(ZENDNN_WERROR "treat warnings as errors" OFF)

set(ZENDNN_TEST_SET "CI" CACHE STRING
    "specifies testing targets coverage. Supports CI, CI_NO_CORR, NIGHTLY.

    When CI option is set, it enables a subset of test targets to run. When
    CI_NO_CORR option is set, it enables same coverage as for CI option, but
    switches off correctness validation for benchdnn targets. When NIGHTLY
    option is set, it enables a broader set of test targets to run.")

set(ZENDNN_INSTALL_MODE "DEFAULT" CACHE STRING
    "specifies installation mode; supports DEFAULT or BUNDLE.

    When BUNDLE option is set ZENDNN will be installed as a bundle
    which contains examples and benchdnn.")

set(ZENDNN_CODE_COVERAGE "OFF" CACHE STRING
    "specifies which supported tool for code coverage will be used
    Currently only gcov supported")
if(NOT "${ZENDNN_CODE_COVERAGE}" MATCHES "^(OFF|GCOV)$")
    message(FATAL_ERROR "Unsupported code coverage tool: ${ZENDNN_CODE_COVERAGE}")
endif()

set(ZENDNN_DPCPP_HOST_COMPILER "DEFAULT" CACHE STRING
    "specifies host compiler for Intel oneAPI DPC++ Compiler")

set(ZENDNN_LIBRARY_NAME "amdZenDNN" CACHE STRING
    "specifies name of the library. For example, user can use this variable to
     specify a custom library names for CPU and GPU configurations to safely
     include them into their CMake project via add_subdirectory")

message(STATUS "ZENDNN_LIBRARY_NAME: ${ZENDNN_LIBRARY_NAME}")

set(ZENDNN_ENABLE_WORKLOAD "INFERENCE" CACHE STRING
    "Specifies a set of functionality to be available at build time. Designed to
    decrease the final memory disk footprint of the shared object or application
    statically linked against the library. Valid values:
    - TRAINING (the default). Includes all functionality to be enabled.
    - INFERENCE. Includes only forward propagation kind functionality and their
      dependencies.")
if(NOT "${ZENDNN_ENABLE_WORKLOAD}" MATCHES "^(TRAINING|INFERENCE)$")
    message(FATAL_ERROR "Unsupported workload type: ${ZENDNN_ENABLE_WORKLOAD}")
endif()

set(ZENDNN_ENABLE_PRIMITIVE "ALL" CACHE STRING
    "Specifies a set of primitives to be available at build time. Valid values:
    - ALL (the default). Includes all primitives to be enabled.
    - <PRIMITIVE_NAME>. Includes only the selected primitive to be enabled.
      Possible values are: BATCH_NORMALIZATION, BINARY, CONCAT, CONVOLUTION,
      DECONVOLUTION, ELTWISE, INNER_PRODUCT, LAYER_NORMALIZATION, LRN, MATMUL,
      POOLING, PRELU, REDUCTION, REORDER, RESAMPLING, RNN, SHUFFLE, SOFTMAX,
      SUM.
    - <PRIMITIVE_NAME>;<PRIMITIVE_NAME>;... Includes only selected primitives to
      be enabled at build time. This is treated as CMake string, thus, semicolon
      is a mandatory delimiter between names. This is the way to specify several
      primitives to be available in the final binary.")

set(ZENDNN_ENABLE_PRIMITIVE_CPU_ISA "ALL" CACHE STRING
    "Specifies a set of implementations using specific CPU ISA to be available
    at build time. Regardless of value chosen, compiler-based optimized
    implementations will always be available. Valid values:
    - ALL (the default). Includes all ISA to be enabled.
    - <ISA_NAME>. Includes selected and all \"less\" ISA to be enabled.
      Possible values are: SSE41, AVX2, AVX512, AMX. The linear order is
      SSE41 < AVX2 < AVX512 < AMX. It means that if user selects, e.g. AVX2 ISA,
      SSE41 implementations will also be available at build time.")

set(ZENDNN_ENABLE_PRIMITIVE_GPU_ISA "ALL" CACHE STRING
    "Specifies a set of implementations using specific GPU ISA to be available
    at build time. Regardless of value chosen, reference OpenCL-based
    implementations will always be available. Valid values:
    - ALL (the default). Includes all ISA to be enabled.
    - <ISA_NAME>;<ISA_NAME>;... Includes only selected ISA to be enabled.
      Possible values are: GEN9, GEN11, XELP, XEHP, XEHPG, XEHPC.")

# =============
# Optimizations
# =============

set(ZENDNN_ARCH_OPT_FLAGS "HostOpts" CACHE STRING
    "specifies compiler optimization flags (see below for more information).
    If empty default optimization level would be applied which depends on the
    compiler being used.

    - For Intel C++ Compilers the default option is `-xSSE4.1` which instructs
      the compiler to generate the code for the processors that support SSE4.1
      instructions. This option would not allow to run the library on older
      architectures.

    - For GNU* Compiler Collection and Clang, the default option is `-msse4.1` which
      behaves similarly to the description above.

    - For all other cases there are no special optimizations flags.

    If the library is to be built for generic architecture (e.g. built by a
    Linux distributive maintainer) one may want to specify ZENDNN_ARCH_OPT_FLAGS=\"\"
    to not use any host specific instructions")

option(ZENDNN_EXPERIMENTAL
    "Enables experimental features in ZENDNN.
    When enabled, each experimental feature has to be individually selected
    using environment variables."
    OFF) # disabled by default

# ======================
# Profiling capabilities
# ======================

# TODO: restore default to ON after the issue with linking C files by 
# Intel oneAPI DPC++ Compiler is fixed. Currently this compiler issues a warning
# when linking object files built from C and C++ sources.
option(ZENDNN_ENABLE_JIT_PROFILING
    "Enable registration of ZENDNN kernels that are generated at
    runtime with VTune Amplifier (on by default). Without the
    registrations, VTune Amplifier would report data collected inside
    the kernels as `outside any known module`."
    OFF)

option(ZENDNN_ENABLE_ITT_TASKS
    "Enable ITT Tasks tagging feature and tag all primitive execution 
    (on by default). VTune Amplifier can group profiling results based 
    on those ITT tasks and show corresponding timeline information."
    OFF)

# ===================
# Engine capabilities
# ===================

set(ZENDNN_CPU_RUNTIME "OMP" CACHE STRING
    "specifies the threading runtime for CPU engines;
    supports OMP (default), TBB or DPCPP (DPC++ CPU engines).

    To use Threading Building Blocks (TBB) one should also
    set TBBROOT (either environment variable or CMake option) to the library
    location.")
if(NOT "${ZENDNN_CPU_RUNTIME}" MATCHES "^(NONE|OMP|TBB|SEQ|THREADPOOL|DPCPP|SYCL)$")
    message(FATAL_ERROR "Unsupported CPU runtime: ${ZENDNN_CPU_RUNTIME}")
endif()

set(_ZENDNN_TEST_THREADPOOL_IMPL "STANDALONE" CACHE STRING
    "specifies which threadpool implementation to use when
    ZENDNN_CPU_RUNTIME=THREADPOOL is selected. Valid values: STANDALONE, EIGEN,
    TBB")
if(NOT "${_ZENDNN_TEST_THREADPOOL_IMPL}" MATCHES "^(STANDALONE|TBB|EIGEN)$")
    message(FATAL_ERROR
        "Unsupported threadpool implementation: ${_ZENDNN_TEST_THREADPOOL_IMPL}")
endif()

set(TBBROOT "" CACHE STRING
    "path to Thread Building Blocks (TBB).
    Use this option to specify TBB installation locaton.")

set(ZENDNN_GPU_RUNTIME "NONE" CACHE STRING
    "specifies the runtime to use for GPU engines.
    Can be NONE (default; no GPU engines), OCL (OpenCL GPU engines)
    or DPCPP (DPC++ GPU engines).

    Using OpenCL for GPU requires setting OPENCLROOT if the libraries are
    installed in a non-standard location.")
if(NOT "${ZENDNN_GPU_RUNTIME}" MATCHES "^(OCL|NONE|DPCPP|SYCL)$")
    message(FATAL_ERROR "Unsupported GPU runtime: ${ZENDNN_GPU_RUNTIME}")
endif()

set(ZENDNN_GPU_VENDOR "INTEL" CACHE STRING
    "specifies target GPU vendor for GPU engines.
    Can be INTEL (default) or NVIDIA.")
if(NOT "${ZENDNN_GPU_VENDOR}" MATCHES "^(INTEL|NVIDIA)$")
    message(FATAL_ERROR "Unsupported GPU vendor: ${ZENDNN_GPU_VENDOR}")
endif()

set(OPENCLROOT "" CACHE STRING
    "path to Intel SDK for OpenCL applications.
    Use this option to specify custom location for OpenCL.")

# TODO: move logic to other cmake files?
# Shortcuts for SYCL/DPC++
if(ZENDNN_CPU_RUNTIME STREQUAL "DPCPP" OR ZENDNN_CPU_RUNTIME STREQUAL "SYCL")
    set(ZENDNN_CPU_SYCL true)
else()
    set(ZENDNN_CPU_SYCL false)
endif()

if(ZENDNN_GPU_RUNTIME STREQUAL "DPCPP" OR ZENDNN_GPU_RUNTIME STREQUAL "SYCL")
    set(ZENDNN_GPU_SYCL true)
    set(ZENDNN_SYCL_CUDA OFF)
    if(ZENDNN_GPU_VENDOR STREQUAL "NVIDIA")
        set(ZENDNN_SYCL_CUDA ON)
    endif()
else()
    set(ZENDNN_GPU_SYCL false)
endif()

if(ZENDNN_CPU_SYCL OR ZENDNN_GPU_SYCL)
    set(ZENDNN_WITH_SYCL true)
else()
    set(ZENDNN_WITH_SYCL false)
endif()

# =============
# Miscellaneous
# =============

option(BENCHDNN_USE_RDPMC
    "enables rdpms counter to report precise cpu frequency in benchdnn.
    CAUTION: may not work on all cpus (hence disabled by default)"
    OFF) # disabled by default

# =========================
# Developer and debug flags
# =========================

set(ZENDNN_USE_CLANG_SANITIZER "" CACHE STRING
    "instructs build system to use a Clang sanitizer. Possible values:
    Address: enables AddressSanitizer
    Leak: enables LeakSanitizer
    Memory: enables MemorySanitizer
    MemoryWithOrigin: enables MemorySanitizer with origin tracking
    Thread: enables ThreadSanitizer
    Undefined: enables UndefinedBehaviourSanitizer
    This feature is experimental and is only available on Linux.")

option(ZENDNN_ENABLE_MEM_DEBUG "enables memory-related debug functionality,
    such as buffer overflow (default) and underflow, using gtests and benchdnn.
    Additionaly, this option enables testing of out-of-memory handling by the
    library, such as failed memory allocations, using primitive-related gtests.
    This feature is experimental and is only available on Linux." OFF)

set(ZENDNN_USE_CLANG_TIDY "NONE" CACHE STRING
    "Instructs build system to use clang-tidy. Valid values:
    - NONE (default)
      Clang-tidy is disabled.
    - CHECK
      Enables checks from .clang-tidy.
    - FIX
      Enables checks from .clang-tidy and fix found issues.
    This feature is only available on Linux.")

option(ZENDNN_ENABLE_STACK_CHECKER "enables stack checker that can be used to get
    information about stack consumption for a particular library entry point.
    This feature is only available on Linux (see src/common/stack_checker.hpp
    for more details).
    Note: This option requires enabling concurrent scratchpad
    (ZENDNN_ENABLE_CONCURRENT_EXEC)." OFF)

# =============================
# External BLAS library options
# =============================

set(ZENDNN_BLAS_VENDOR "NONE" CACHE STRING
    "Use an external BLAS library. Valid values:
      - NONE (default)
        Use in-house implementation.
      - MKL
        Intel Math Kernel Library (Intel MKL)
        (https://software.intel.com/content/www/us/en/develop/tools/math-kernel-library.html)
      - OPENBLAS
        (https://www.openblas.net)
      - ARMPL
        Arm Performance Libraries
        (https://developer.arm.com/tools-and-software/server-and-hpc/downloads/arm-performance-libraries)
      - ANY
        FindBLAS will search default library paths for a known BLAS installation.")

# ==============================================
# AArch64 optimizations with Arm Compute Library
# ==============================================

option(ZENDNN_AARCH64_USE_ACL "Enables use of AArch64 optimised functions
    from Arm Compute Library.
    This is only supported on AArch64 builds and assumes there is a
    functioning Compute Library build available at the location specified by the
    environment variable ACL_ROOT_DIR." OFF)

# ==============================================
# ZenDNN Optimizations
# ==============================================

option(ZENDNN_PATH_ENABLE "Enables ZenDNN specifc code.
    This needs to be cleaned later and this flag may not be needed." ON)

option(ZENDNN_BIAS_ENABLE "Enables ZenDNN BIAS." ON)

option(ZENDNN_BLIS_EXPERT_ENABLE "Enables ZenDNN BLIS_EXPERT interface." ON)

option(ZENDNN_DIRECT_CONV_ENABLE "Enables ZENDNN_DIRECT_CONV specifc code." ON)

option(ZENDNN_BLOCKED_POOLING_ENABLE "Enables ZENDNN_BLOCKED_POOLING specifc
    code." ON)
option(ZENDNN_LIBM_ENABLE "Enables ZenDNN LibM specific code specifc
    code." ON)
option(ZENDNN_USE_LOCAL_BLIS "Use locally installed BLIS.
    export ZENDNN_BLIS_PATH to use it." ON)
option(ZENDNN_USE_LOCAL_LIBM "Use locally installed LIBM.
    export ZENDNN_LIBM_PATH to use it." ON)
