cmake_minimum_required(VERSION 3.20)

project(BTPlayer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(BTP_Backend
    main.cpp
)

include_directories(thirdparty)
