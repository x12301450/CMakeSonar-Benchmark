file(GLOB_RECURSE CPP_REDIS_SRCS
     ${PROJECT_SOURCE_DIR}/third_party/cpp_redis/sources/*.cpp
     ${PROJECT_SOURCE_DIR}/third_party/tacopie/sources/*.cpp)
set(CPP_REDIS_INCLUDES ${PROJECT_SOURCE_DIR}/third_party/cpp_redis/includes
                       ${PROJECT_SOURCE_DIR}/third_party/tacopie/includes)
