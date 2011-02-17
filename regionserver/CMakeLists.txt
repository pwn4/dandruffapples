add_executable(regionserver
  regionserver.cpp
  areaengine.cpp
  )

pkg_check_modules(GTK REQUIRED gtk+-2.0)

link_directories(${GTKMM_LIBRARY_DIRS})

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})

target_link_libraries(regionserver common ${ImageMagick_LIBRARIES} ${GTK_LIBRARIES})
