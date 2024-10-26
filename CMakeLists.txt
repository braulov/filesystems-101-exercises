cmake_minimum_required(VERSION 3.29)
project(filesystems_101_exercises C)

set(CMAKE_C_STANDARD 11)

include_directories(00-ps)

add_executable(filesystems_101_exercises
        00-ps/callbacks.c
        00-ps/main.c
        00-ps/solution.c
        00-ps/solution.h)
