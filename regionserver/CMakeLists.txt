add_executable(regionserver
  regionserver.cpp
  areaengine.cpp
  )

target_link_libraries(regionserver common drawer)

find_package(OpenCL)

if(OPENCL_FOUND)
  message(STATUS "Building with GPU-acceleration-capable simulation code.")
  include_directories(${OPENCL_INCLUDE_DIRS})
  target_link_libraries(regionserver ${OPENCL_LIBRARIES})
  add_definitions(-DOPENCL)
else(OPENCL_FOUND)
  message(STATUS "Building with old simulation code.")
endif(OPENCL_FOUND)
