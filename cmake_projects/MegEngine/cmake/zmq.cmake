include(ExternalProject)
include(GNUInstallDirs)

set(ZMQ_DIR
    ${PROJECT_SOURCE_DIR}/third_party/libzmq
    CACHE STRING "ZMQ directory")
set(ZMQ_BUILD_DIR ${PROJECT_BINARY_DIR}/third_party/libzmq)
set(ZMQ_LIB ${ZMQ_BUILD_DIR}/${CMAKE_INSTALL_LIBDIR}/libzmq.a)

ExternalProject_Add(
  zmq
  SOURCE_DIR ${ZMQ_DIR}
  PREFIX ${ZMQ_BUILD_DIR}
  CMAKE_ARGS -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}
             -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
             -DCMAKE_INSTALL_PREFIX=${ZMQ_BUILD_DIR}
             -DWITH_PERF_TOOL=OFF
             -DZMQ_BUILD_TESTS=OFF
             -DENABLE_CPACK=OFF
             -DENABLE_CURVE=OFF
  BUILD_BYPRODUCTS ${ZMQ_LIB})

set(ZMQ_INC ${ZMQ_BUILD_DIR}/include)
include_directories(${ZMQ_INC})
file(MAKE_DIRECTORY ${ZMQ_INC})

add_library(libzmq STATIC IMPORTED GLOBAL)
add_dependencies(libzmq zmq)
set_target_properties(libzmq PROPERTIES IMPORTED_LOCATION ${ZMQ_LIB}
                                        INTERFACE_INCLUDE_DIRECTORIES ${ZMQ_INC})
