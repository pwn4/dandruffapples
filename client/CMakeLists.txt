add_executable(client
  client.cpp
  clientviewer.cpp
  )

add_executable(remotecontrol
  remotecontrol.cpp 
  )

add_library(ai_forager SHARED
  ai_forager.cpp client_ai.cpp
  )
add_library(ai_scared SHARED
  ai_scared.cpp client_ai.cpp
  )

pkg_check_modules(GTK REQUIRED gtk+-2.0)

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(client common drawer ${GTK_LIBRARIES})
target_link_libraries(remotecontrol common ${GTK_LIBRARIES})
