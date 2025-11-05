cmake_minimum_required(VERSION 4.0.0)
project(gol VERSION 0.1.0)

set(VCPKG_MANIFEST_MODE ON)

set(CMAKE_CXX_STANDARD 23) # Requires C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib 3.0 REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} raylib)

#if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
#    add_custom_command(
#        TARGET gol
#        POST_BUILD
#        COMMAND cv2pdb gol.exe
#        COMMENT "Running MyApp after build"
#    )
#endif()
