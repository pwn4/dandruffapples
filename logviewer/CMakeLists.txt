add_executable(logviewer
  gui.cpp
  home.cpp
  logviewer.cpp
  main.cpp
  puck.cpp
  robot.cpp
  )

#we are using an obsolete package here
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
include_directories(${GLUT_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

target_link_libraries(logviewer common ${GLUT_glut_LIBRARY} ${OPENGL_LIBRARIES})
