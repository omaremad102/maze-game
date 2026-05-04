cmake_minimum_required(VERSION 3.20)
project(MazeScape3D VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(MazeScape3D
  src/main.cpp
  src/Maze.cpp
  src/Solver.cpp
)

target_include_directories(MazeScape3D PRIVATE src)
target_link_libraries(MazeScape3D PRIVATE raylib)

if (WIN32)
  target_link_libraries(MazeScape3D PRIVATE winmm)
endif()
