cmake_minimum_required(VERSION 3.9)

project(tethys VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

add_subdirectory(external/mn EXCLUDE_FROM_ALL)
add_subdirectory(vm)
add_subdirectory(as)
add_subdirectory(tas)
add_subdirectory(playground)
add_subdirectory(unittest)