# MIT License
#
# Copyright (c) 2015-2024 The ViaDuck Project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

# build openssl locally

# includes
include(ProcessorCount)
include(ExternalProject)

# find packages
find_package(Python COMPONENTS Interpreter REQUIRED)

# used to apply various patches to OpenSSL
find_program(PATCH_PROGRAM patch)
if (NOT PATCH_PROGRAM)
    message(FATAL_ERROR "Cannot find patch utility. This is only required for Android cross-compilation but due to script complexity "
                        "the requirement is always enforced")
endif()

# set variables
ProcessorCount(NUM_JOBS)
set(OS "UNIX")

if (OPENSSL_BUILD_HASH)
    set(OPENSSL_CHECK_HASH URL_HASH SHA256=${OPENSSL_BUILD_HASH})
endif()

# if already built, do not build again
if (EXISTS ${OPENSSL_PREFIX})
    message(WARNING "Not building OpenSSL again. Remove ${OPENSSL_PREFIX} for rebuild")
else()
    if (NOT OPENSSL_BUILD_VERSION)
        message(FATAL_ERROR "You must specify OPENSSL_BUILD_VERSION!")
    endif()

    if (WIN32 AND NOT CROSS)
        # yep, windows needs special treatment, but neither cygwin nor msys, since they provide an UNIX-like environment
        
        if (MINGW)
            set(OS "WIN32")
            message(WARNING "Building on windows is experimental")
            
            find_program(MSYS_BASH "bash.exe" PATHS "C:/Msys/" "C:/MinGW/msys/" PATH_SUFFIXES "/1.0/bin/" "/bin/"
                    DOC "Path to MSYS installation")
            if (NOT MSYS_BASH)
                message(FATAL_ERROR "Specify MSYS installation path")
            endif(NOT MSYS_BASH)
            
            set(MINGW_MAKE ${CMAKE_MAKE_PROGRAM})
            message(WARNING "Assuming your make program is a sibling of your compiler (resides in same directory)")
        elseif(NOT (CYGWIN OR MSYS))
            message(FATAL_ERROR "Unsupported compiler infrastructure")
        endif(MINGW)
        
        set(MAKE_PROGRAM ${CMAKE_MAKE_PROGRAM})
    elseif(NOT UNIX)
        message(FATAL_ERROR "Unsupported platform")
    else()
        # for OpenSSL we can only use GNU make, no exotic things like Ninja (MSYS always uses GNU make)
        find_program(MAKE_PROGRAM make)
    endif()

    # on windows we need to replace path to perl since CreateProcess(..) cannot handle unix paths
    if (WIN32 AND NOT CROSS)
        set(PERL_PATH_FIX_INSTALL sed -i -- 's/\\/usr\\/bin\\/perl/perl/g' Makefile)
    else()
        set(PERL_PATH_FIX_INSTALL true)
    endif()

    # CROSS and CROSS_ANDROID cannot both be set (because of internal reasons)
    if (CROSS AND CROSS_ANDROID)
        # if user set CROSS_ANDROID and CROSS we assume he wants CROSS_ANDROID, so set CROSS to OFF
        set(CROSS OFF)
    endif()

    if (CROSS_ANDROID)
        set(OS "LINUX_CROSS_ANDROID")
    endif()

    # python helper script for corrent building environment
    set(BUILD_ENV_TOOL ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/building_env.py
        --bash "${MSYS_BASH}" --make "${MINGW_MAKE}" --envfile "${CMAKE_CURRENT_BINARY_DIR}/buildenv.txt" ${OS})

    # user-specified modules
    set(CONFIGURE_OPENSSL_MODULES ${OPENSSL_MODULES})

    # additional configure script parameters
    set(CONFIGURE_OPENSSL_PARAMS --libdir=lib)
    if (OPENSSL_DEBUG_BUILD)
        set(CONFIGURE_OPENSSL_PARAMS "${CONFIGURE_OPENSSL_PARAMS} no-asm -g3 -O0 -fno-omit-frame-pointer -fno-inline-functions")
    endif()
    if (OPENSSL_RPATH)
        # ridiculous escaping required to pass through cmake, one shell, one makefile and another shell.
        # \\\\ in shell, \\ in makefile
        string(REPLACE "\\" "\\\\\\\\" OPENSSL_RPATH_ESCAPED ${OPENSSL_RPATH})
        # \\$\$ in shell, \$$ in makefile
        string(REPLACE "\$" "\\\\\$\\\$" OPENSSL_RPATH_ESCAPED ${OPENSSL_RPATH_ESCAPED}) # \$$ in makefile
        set(CONFIGURE_OPENSSL_PARAMS "${CONFIGURE_OPENSSL_PARAMS} -Wl,-rpath=${OPENSSL_RPATH_ESCAPED}")
    endif()
    
    # set install command depending of choice on man page generation
    if (OPENSSL_INSTALL_MAN)
        set(INSTALL_OPENSSL_MAN "install_docs")
    endif()
    
    # disable building tests
    if (NOT OPENSSL_ENABLE_TESTS)
        set(CONFIGURE_OPENSSL_MODULES ${CONFIGURE_OPENSSL_MODULES} no-tests)
        set(COMMAND_TEST "true")
    endif()

    # cross-compiling
    if (CROSS)
        set(COMMAND_CONFIGURE ./Configure ${CONFIGURE_OPENSSL_PARAMS} --cross-compile-prefix=${CROSS_PREFIX} ${CROSS_TARGET} 
            ${CONFIGURE_OPENSSL_MODULES} --prefix=/usr/local/)
        set(COMMAND_TEST "true")
    elseif(CROSS_ANDROID)
        # required environment configuration is already set (by e.g. ndk) so no need to fiddle around with all the OpenSSL options ...
        if (NOT ANDROID)
            message(FATAL_ERROR "Use NDK cmake toolchain or cmake android autoconfig")
        endif()
        
        # arch options
        if (ARMEABI_V7A)
            set(OPENSSL_PLATFORM "arm")
            set(CONFIGURE_OPENSSL_PARAMS ${CONFIGURE_OPENSSL_PARAMS} "-march=armv7-a")
        else()
            if (CMAKE_ANDROID_ARCH_ABI MATCHES "arm64-v8a")
                set(OPENSSL_PLATFORM "arm64")
            else()
                set(OPENSSL_PLATFORM ${CMAKE_ANDROID_ARCH_ABI})
            endif()
        endif()
        
        # collect options to pass via ENV to openssl configure
        set(FORWARD_ANDROID_NDK "${ANDROID_NDK}")
        # silence warnings about unused arguments (Clang specific)
        set(FORWARD_CFLAGS "${CMAKE_C_FLAGS} -Qunused-arguments")
        set(FORWARD_CXXFLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments")
        set(FORWARD_LDFLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
        set(FORWARD_PATH "${ANDROID_TOOLCHAIN_ROOT}/bin/:${ANDROID_TOOLCHAIN_ROOT}/${ANDROID_TOOLCHAIN_NAME}/bin/")
        
        # Android specific configuration options
        set(CONFIGURE_OPENSSL_MODULES ${CONFIGURE_OPENSSL_MODULES} no-hw)
        
        set(COMMAND_CONFIGURE ./Configure android-${OPENSSL_PLATFORM} ${CONFIGURE_OPENSSL_PARAMS} ${CONFIGURE_OPENSSL_MODULES})
        set(COMMAND_TEST "true")
    else()                   # detect host system automatically
        set(COMMAND_CONFIGURE ./config ${CONFIGURE_OPENSSL_PARAMS} ${CONFIGURE_OPENSSL_MODULES})
        
        if (NOT COMMAND_TEST)
            set(COMMAND_TEST ${BUILD_ENV_TOOL} <SOURCE_DIR> -- ${MAKE_PROGRAM} test)
        endif()
    endif()

    # build OPENSSL_PATCH_COMMAND
    include(PatchOpenSSL)

    # add openssl target
    ExternalProject_Add(openssl
        URL https://mirror.viaduck.org/openssl/openssl-${OPENSSL_BUILD_VERSION}.tar.gz
        ${OPENSSL_CHECK_HASH}
        UPDATE_COMMAND ""

        CONFIGURE_COMMAND ${BUILD_ENV_TOOL} <SOURCE_DIR> -- ${COMMAND_CONFIGURE}
        ${OPENSSL_PATCH_COMMAND}

        BUILD_COMMAND ${BUILD_ENV_TOOL} <SOURCE_DIR> -- ${MAKE_PROGRAM} -j ${NUM_JOBS}
        BUILD_BYPRODUCTS ${OPENSSL_BYPRODUCTS}

        TEST_BEFORE_INSTALL 1
        TEST_COMMAND ${COMMAND_TEST}

        INSTALL_COMMAND ${BUILD_ENV_TOOL} <SOURCE_DIR> -- ${PERL_PATH_FIX_INSTALL}
        COMMAND ${BUILD_ENV_TOOL} <SOURCE_DIR> -- ${MAKE_PROGRAM} DESTDIR=${OPENSSL_PREFIX} install_sw ${INSTALL_OPENSSL_MAN}
        COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} ${CMAKE_BINARY_DIR}                    # force CMake-reload

        LOG_INSTALL 1
    )

    # write all "FORWARD_" variables with escaped quotes to file, is picked up by python script
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        if (_variableName MATCHES "^FORWARD_")
            string(REPLACE "FORWARD_" "" _envName ${_variableName})
            string(REPLACE "\"" "\\\"" _envValue "${${_variableName}}")
            set(OUT_FILE "${OUT_FILE}${_envName}=\"${_envValue}\"\n")
        endif()
    endforeach()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/buildenv.txt ${OUT_FILE})
endif()
