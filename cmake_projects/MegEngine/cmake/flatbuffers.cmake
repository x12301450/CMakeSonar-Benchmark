if(MGE_USE_SYSTEM_LIB)
  find_package(Flatbuffers REQUIRED)
  message(STATUS "Using system provided Flatbuffers ${Flatbuffers_VERSION}")
  include(cmake/BuildFlatBuffers.cmake)
  return()
endif()
if(MSVC OR WIN32)
  message(DEBUG "add flags flatc for clang-cl build")
  set(FLATC_FLAGS "")
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=unknown-argument -Wno-error=c++98-compat -Wno-error=reserved-id-macro"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=sign-conversion -Wno-error=exceptions -Wno-error=argument-outside-range"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=delete-non-virtual-dtor -Wno-error=ignored-attributes -Wno-error=format"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=sign-compare -Wno-error=unused-private-field -Wno-error=braced-scalar-init"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=return-type-c-linkage -Wno-error=invalid-noreturn -Wno-error=c++98-compat-pedantic"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=extra-semi-stmt -Wno-error=missing-prototypes -Wno-error=documentation-unknown-command"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=missing-variable-declarations  -Wno-error=nonportable-system-include-path"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=exit-time-destructors -Wno-error=unused-macros -Wno-error=global-constructors"
  )
  set(FLATC_FLAGS
      "${FLATC_FLAGS} -Wno-error=switch-enum -Wno-error=missing-noreturn -Wno-error=float-equal"
  )
  if(${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "11.0.0")
    set(FLATC_FLAGS
        "${FLATC_FLAGS} -Wno-error=suggest-override -Wno-error=suggest-destructor-override"
    )
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLATC_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLATC_FLAGS}")
endif()

option(FLATBUFFERS_BUILD_TESTS "" OFF)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/flatbuffers
                 ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers)
add_library(flatbuffers::flatbuffers ALIAS flatbuffers)
