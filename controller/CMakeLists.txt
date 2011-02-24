add_executable(controller
  controller.cpp
  )

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(controller common ${GTK_LIBRARIES})
