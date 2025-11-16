cmake_minimum_required(VERSION 4.0.0)
project(gol VERSION 0.1.0 DESCRIPTION "Game of Life")

set(CMAKE_CXX_STANDARD 23) # Requires C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXTERN_DIR extern)

# Add the Raylib subdirectory, which will build the raylib library
add_subdirectory(${EXTERN_DIR}/raylib)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} raylib)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_custom_command(
        TARGET gol
        POST_BUILD
        COMMAND cv2pdb -C gol.exe
        COMMENT "Creating PDB files"
    )
endif()
