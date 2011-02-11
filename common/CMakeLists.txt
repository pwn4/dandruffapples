if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_definitions(-std=c++0x)

find_package(Protobuf REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROTOBUF_INCLUDE_DIRS})

find_package(Threads REQUIRED)

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS
  claim.proto
  claimteam.proto
  clientrobot.proto
  regionrender.proto
  puckstack.proto
  serverrobot.proto
  timestep.proto
  worldinfo.proto
  )

add_library(common
  ${PROTO_SRCS}
  ${PROTO_HDRS}
  net.cpp
  messagereader.cpp
  messagewriter.cpp
  messagequeue.cpp
  parseconf.cpp
  helper.cpp
  areaengine.cpp
  )

target_link_libraries(common ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
