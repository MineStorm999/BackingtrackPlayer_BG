cmake_minimum_required(VERSION 3.20)

project(BTPlayer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0.1
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UDISKS2 REQUIRED IMPORTED_TARGET udisks2)

# 3. Eigen verfügbar machen
FetchContent_MakeAvailable(eigen)

add_executable(BTP_Backend
    main.cpp
    Track.cpp
    Song.cpp
)

target_link_libraries(BTP_Backend PRIVATE Eigen3::Eigen PkgConfig::UDISKS2)
include_directories(thirdparty)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    target_compile_options(BTP_Backend PRIVATE -O3 -mavx2) # Am PC: AVX2 aktivieren
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(BTP_Backend PRIVATE -O3 -mcpu=cortex-a76) # Am Pi 5: NEON aktivieren
endif()
