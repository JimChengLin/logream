cmake_minimum_required(VERSION 3.10)
project(logream)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")

add_executable(logream main.cpp
        bench/logream_compress_bench.cpp
        bench/logream_lite_bench.cpp
        src/bloom.h
        src/coding.cpp src/coding.h
        src/crc32c.cpp src/crc32c.h
        src/divsufsort.cpp src/divsufsort.h
        src/logream.h
        src/logream_compress.cpp src/logream_compress.h
        src/logream_lite.cpp src/logream_lite.h
        src/prefetch.h
        src/slice.h
        )