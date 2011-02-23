add_executable(regionserver
  regionserver.cpp
  areaengine.cpp
  )

pkg_check_modules(GTK REQUIRED gtk+-2.0)

link_directories(${GTK_LIBRARY_DIRS})
include_directories(${GTK_INCLUDE_DIRS})

target_link_libraries(regionserver common drawer ${GTK_LIBRARIES})

