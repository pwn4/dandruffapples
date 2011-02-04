add_executable(regionserver
  regionserver.cpp
  net.cpp
  ../common/functions.cpp
  )

target_link_libraries(regionserver common)
