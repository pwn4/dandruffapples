add_executable(pngviewer
  pngviewer.cpp
  )

find_package(GTK2 COMPONENTS gtk glade REQUIRED)

link_directories(${GTKMM_LIBRARY_DIRS})

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})

target_link_libraries(pngviewer common ${ImageMagick_LIBRARIES} ${GTKMM_LIBRARIES})
