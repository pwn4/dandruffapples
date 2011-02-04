add_executable(clockserver
    clockserver.cpp
    ../common/functions.cpp
  )

target_link_libraries(clockserver common)
