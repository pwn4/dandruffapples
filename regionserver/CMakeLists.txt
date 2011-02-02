add_executable(regionserver
  regionserver.cpp
  net.cpp
  )

target_link_libraries(regionserver common)
