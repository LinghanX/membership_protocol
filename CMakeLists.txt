cmake_minimum_required(VERSION 3.7)
project(prj2)

include_directories(include)
set(CMAKE_CXX_STANDARD 11)

add_executable(prj2 main.cpp parse.h process.h message.h parse.cpp process.cpp message.cpp)