if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_definitions(-std=c++0x)

pkg_check_modules(GTK REQUIRED gtk+-2.0)

find_package(Protobuf REQUIRED)

link_directories(${GTK_LIBRARY_DIRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROTOBUF_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})

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
  helper.h
  )

target_link_libraries(common ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${GTK_LIBRARIES})
