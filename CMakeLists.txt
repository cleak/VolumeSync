cmake_minimum_required(VERSION 3.8)
project(VolumeSync)

set(CMAKE_CXX_STANDARD 17)

set(SOURCES main.cpp Audio.cpp Audio.h)
add_executable(VolumeSync ${SOURCES})

target_link_libraries(VolumeSync ole32.lib uuid.lib)
