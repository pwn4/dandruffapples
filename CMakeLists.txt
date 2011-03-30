project(dandruffapples)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

SET(CMAKE_CXX_FLAGS_PROFILE "-g -pg")


find_package(PkgConfig)

add_definitions(-Wall -Wextra -Wno-unused-parameter)

add_subdirectory(common)

add_subdirectory(regionserver)
add_subdirectory(clockserver)
add_subdirectory(controller)
add_subdirectory(client)
add_subdirectory(worldviewer)
add_subdirectory(logmerger)
#add_subdirectory(logviewer)
