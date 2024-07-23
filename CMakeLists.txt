cmake_minimum_required(VERSION 3.15)

project(AlsaTest VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ALSA REQUIRED)

add_executable(AlsaTest src/main.cpp)
target_link_libraries(AlsaTest PRIVATE ${ALSA_LIBRARIES})
target_include_directories(AlsaTest PRIVATE ${ALSA_INCLUDE_DIRS})

install(TARGETS AlsaTest DESTINATION bin)
