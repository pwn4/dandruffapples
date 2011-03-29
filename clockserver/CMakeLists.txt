add_executable(clockserver
    clockserver.cpp
  )

target_link_libraries(clockserver common ${GTK_LIBRARIES})
