add_executable(client
  client.cpp
  )

add_executable(remotecontrol
  remotecontrol.cpp 
  )

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(client common ${GTK_LIBRARIES})
target_link_libraries(remotecontrol common ${GTK_LIBRARIES})
