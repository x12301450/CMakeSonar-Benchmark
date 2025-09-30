if(MGE_USE_SYSTEM_LIB)
  find_package(dnnl)
  if(dnnl_FOUND)
    message(STATUS "Using system provided MKL-DNN.")
    set(MGE_USE_SYSTEM_MKLDNN ON)
    return()
  endif()
endif()
option(DNNL_BUILD_TESTS "" OFF)
option(DNNL_BUILD_EXAMPLES "" OFF)
# we do not want to use OMP now, so config to CPU mode if set to OMP, some dnnl algo
# will be more fast
set(DNNL_CPU_RUNTIME
    "SEQ"
    CACHE STRING "config dnnl to DNNL_RUNTIME_SEQ")
if(MGE_BLAS STREQUAL "MKL")
  option(_DNNL_USE_MKL "" ON)
  set(MKLROOT
      ${MKL_ROOT_DIR}
      CACHE STRING "MKL ROOT FOR DNNL")
  set(MKLLIB libmkl)
else()
  option(_DNNL_USE_MKL "" OFF)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-extra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-extra")
set(DNNL_LIBRARY_TYPE
    STATIC
    CACHE STRING "config dnnl to STATIC")
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/intel-mkl-dnn)
