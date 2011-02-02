if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS timestep.proto)

find_package(Threads REQUIRED)

add_library(common ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(common ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
