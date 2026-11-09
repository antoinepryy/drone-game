cmake_minimum_required(VERSION 3.16)

# Single source of truth for the version number: bump it here.
project(drone_pilot VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core logic (no SDL dependency, unit-testable)
add_library(dronecore STATIC src/drone.c src/command.c src/script.c)
target_include_directories(dronecore PUBLIC src)
target_compile_definitions(dronecore PUBLIC APP_VERSION="${PROJECT_VERSION}")
if(NOT WIN32)
    target_link_libraries(dronecore PUBLIC m)
endif()

# Graphical application
find_package(SDL2 REQUIRED)
add_executable(drone_pilot src/main.c src/render.c src/font.c)
target_link_libraries(drone_pilot PRIVATE dronecore SDL2::SDL2)

# Tests
enable_testing()
add_executable(test_drone tests/test_drone.c)
target_link_libraries(test_drone PRIVATE dronecore)
add_test(NAME test_drone COMMAND test_drone)
