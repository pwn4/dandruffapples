add_executable(regionserver
  regionserver.cpp
  )

target_link_libraries(regionserver common)

find_package(ImageMagick REQUIRED)
include_directories(${ImageMagick_INCLUDE_DIRS})
target_link_libraries(regionserver ${ImageMagick_LIBRARIES})
