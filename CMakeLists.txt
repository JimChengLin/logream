cmake_minimum_required(VERSION 3.10)
project(logream)

set(CMAKE_CXX_STANDARD 17)

add_executable(logream main.cpp
        src/logream.cpp src/logream.h
        )