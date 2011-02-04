project(dandruffapples)

cmake_minimum_required(VERSION 2.8)

add_definitions(-Wall -Wextra)

add_subdirectory(common)

add_subdirectory(regionserver)
add_subdirectory(clockserver)
add_subdirectory(controller)
