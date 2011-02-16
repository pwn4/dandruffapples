add_executable(pngviewer
  pngviewer.cpp
  )

pkg_check_modules(GTK REQUIRED gtk+-2.0)

link_directories(${GTK_LIBRARY_DIRS})

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})

target_link_libraries(pngviewer common ${ImageMagick_LIBRARIES} ${GTK_LIBRARIES})
