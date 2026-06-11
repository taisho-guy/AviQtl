cmake_minimum_required(VERSION 4.3.3)

project(AviQtlRender LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(RENDER_SOURCES
    preview/preview_render_thread.cpp
)

set(RENDER_HEADERS
    preview/preview_render_thread.hpp
)

add_library(AviQtlRender STATIC ${RENDER_SOURCES} ${RENDER_HEADERS})

target_link_libraries(AviQtlRender PRIVATE
    Qt6::Core
    bgfx
    bx
    bimg
)

target_include_directories(AviQtlRender PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
