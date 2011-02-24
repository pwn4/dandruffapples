add_executable(clockserver
    clockserver.cpp
  )

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(clockserver common ${GTK_LIBRARIES})
