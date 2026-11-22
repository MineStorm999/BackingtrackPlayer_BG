cmake_minimum_required(VERSION 3.20)

project(BTPlayer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# eigen (vector arrithmatic)
include(FetchContent)
FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0.1
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDISKS2 REQUIRED IMPORTED_TARGET udisks2)
FetchContent_MakeAvailable(eigen)

# exe
add_executable(BTP_Backend
    main.cpp
    Track.cpp
    Song.cpp
    Show.cpp
    Library.cpp
)

target_link_libraries(BTP_Backend PRIVATE Eigen3::Eigen PkgConfig::UDISKS2)
include_directories(thirdparty)

# compile opts
message("Release build")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    message("x86")
    target_compile_options(BTP_Backend PRIVATE -O3 -mavx2)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message("ARM")
    target_compile_options(BTP_Backend PRIVATE -O3 -mcpu=cortex-a76)
endif()
