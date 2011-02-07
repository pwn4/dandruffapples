add_executable(regionserver
  regionserver.cpp
  )

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS})

target_link_libraries(regionserver common ${ImageMagick_LIBRARIES})
