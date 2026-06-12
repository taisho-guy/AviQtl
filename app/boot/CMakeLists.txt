cmake_minimum_required(VERSION 4.3.3)

project(AviQtlBoot LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(BOOT_SOURCES
    main.cpp
)

add_executable(AviQtl ${BOOT_SOURCES})

target_include_directories(AviQtl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/windowing
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine
)

target_link_libraries(AviQtl PRIVATE
    Qt6::Widgets
    SDL3::SDL3
    AviQtlUI
    AviQtlRuntime
    AviQtlPlatform
    AviQtlRender
    AviQtlEngine
    bgfx
    bx
    bimg
)

if(WIN32 OR LINUX)
    find_package(Vulkan REQUIRED)
    target_link_libraries(AviQtl PRIVATE Vulkan::Vulkan)
elseif(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
    target_link_libraries(AviQtl PRIVATE ${METAL_FRAMEWORK} ${QUARTZ_FRAMEWORK})
    set_source_files_properties(${BOOT_SOURCES} PROPERTIES LANGUAGE OBJCXX)
endif()
