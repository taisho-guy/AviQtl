cmake_minimum_required(VERSION 4.3.3)

project(AviQtlRuntime LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(RUNTIME_SOURCES
    windowing/window_registry.cpp
    windowing/window_coordinator.cpp
)

set(RUNTIME_HEADERS
    windowing/window_registry.hpp
    windowing/window_coordinator.hpp
)

add_library(AviQtlRuntime STATIC ${RUNTIME_SOURCES} ${RUNTIME_HEADERS})

target_link_libraries(AviQtlRuntime PRIVATE
    Qt6::Widgets
    AviQtlUI
    AviQtlPlatform
)

target_include_directories(AviQtlRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
)
