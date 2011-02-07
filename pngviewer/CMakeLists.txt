add_executable(pngviewer
  pngviewer.cpp
  )

pkg_check_modules(GTKMM gtkmm-2.4)
link_directories(${GTKMM_LIBRARY_DIRS})

include_directories(${GTKMM_INCLUDE_DIRS})

target_link_libraries(pngviewer common ${GTKMM_LIBRARIES})

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS})

target_link_libraries(pngviewer common ${ImageMagick_LIBRARIES})
