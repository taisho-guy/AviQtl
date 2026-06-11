cmake_minimum_required(VERSION 4.3.3)

project(AviQtlPlatform LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(PLATFORM_SOURCES
    sdl/sdl_preview_surface.cpp
)

set(PLATFORM_HEADERS
    sdl/sdl_preview_surface.hpp
)

add_library(AviQtlPlatform STATIC ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})

target_link_libraries(AviQtlPlatform PRIVATE
    Qt6::Core
    SDL3::SDL3
    AviQtlRender
)

target_include_directories(AviQtlPlatform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../render
)
