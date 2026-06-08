cmake_minimum_required(VERSION 4.3.3)

project(AviQtlSystem LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(SYSTEM_SOURCES
    main.cpp
    preview/preview.cpp
    window_manager/window_manager.cpp
)

set(SYSTEM_HEADERS
    preview/preview.hpp
    window_manager/window_manager.hpp
)

add_executable(BgfxUtl ${SYSTEM_SOURCES} ${SYSTEM_HEADERS})

target_include_directories(BgfxUtl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/window_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/preview
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
)

target_link_libraries(BgfxUtl PRIVATE
    Qt6::Widgets
    SDL3::SDL3
    AviQtlUI
    bgfx
    bx
    bimg
)

if(WIN32 OR LINUX)
    find_package(Vulkan REQUIRED)
    target_link_libraries(BgfxUtl PRIVATE Vulkan::Vulkan)
elseif(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
    target_link_libraries(BgfxUtl PRIVATE ${METAL_FRAMEWORK} ${QUARTZ_FRAMEWORK})
    set_source_files_properties(${SYSTEM_SOURCES} PROPERTIES LANGUAGE OBJCXX)
endif()
