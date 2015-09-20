cmake_minimum_required(VERSION 3.2)

project(theremin)

add_executable(theremin
  main.cpp)

include_directories(/usr/include/SDL2)

target_link_libraries(theremin SDL2)

set_property(TARGET theremin PROPERTY CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
