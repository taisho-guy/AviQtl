cmake_minimum_required(VERSION 4.3.3)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_LINKER_TYPE MOLD)

project(AviQtl
    VERSION 1.0.0
    LANGUAGES CXX C
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt — Widgets のみ。GuiPrivate は SDL 移行により不要
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Widgets)

# SDL3
find_package(SDL3 REQUIRED)

# プラットフォーム別グラフィクス依存
if(WIN32 OR LINUX)
    find_package(Vulkan REQUIRED)
elseif(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
endif()

# bgfx
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/CMakeLists.txt")
    message(FATAL_ERROR "bgfx.cmake が見つかりません。")
endif()

set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(bgfx.cmake)

set(BGFX_TARGETS bgfx bimg bx)
foreach(tgt IN LISTS BGFX_TARGETS)
    if(TARGET ${tgt})
        target_compile_options(${tgt} PRIVATE "-w" "-fms-extensions")
    endif()
endforeach()

if(TARGET bgfx AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(bgfx PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>"
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include bx/string.h>"
    )
endif()

add_subdirectory(app/engine)
add_subdirectory(app/render)
add_subdirectory(app/platform)
add_subdirectory(app/ui)
add_subdirectory(app/runtime)
add_subdirectory(app/boot)


