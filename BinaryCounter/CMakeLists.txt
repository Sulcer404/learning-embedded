cmake_minimum_required(VERSION 3.16)
project(BinaryCounter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(BinaryCounter src/main.cpp)
