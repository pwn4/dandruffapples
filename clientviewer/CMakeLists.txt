add_executable(clientviewer
  clientviewer.cpp
  )
  
  
pkg_check_modules(GTK REQUIRED gtk+-2.0)

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(clientviewer common ${GTK_LIBRARIES})
