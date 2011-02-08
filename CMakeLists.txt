project(dandruffapples)

cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)

add_definitions(-Wall -Wextra)

add_subdirectory(common)

add_subdirectory(regionserver)
add_subdirectory(clockserver)
add_subdirectory(controller)
add_subdirectory(client)
add_subdirectory(pngviewer)
add_subdirectory(logmerger)
