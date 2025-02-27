cmake_minimum_required(VERSION 3.30)
project(wcs)

set(CMAKE_CXX_STANDARD 23)

add_executable(
    server

    main.cpp
    ServerConfig.h
    ServerRunner.h
)