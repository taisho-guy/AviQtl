cmake_minimum_required(VERSION 4.3.3)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_LINKER_TYPE MOLD)

# プロジェクト定義・グローバルポリシー

project(BgfxUtl 
    VERSION 1.0.0 
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# 依存パッケージの探索と取り込み

find_package(Qt6 6.11.1 REQUIRED COMPONENTS Widgets Gui GuiPrivate)

if(WIN32 OR LINUX)
    find_package(Vulkan REQUIRED)
elseif(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/CMakeLists.txt")
    message(FATAL_ERROR "bgfx.cmake が見つかりません。Gitサブモジュール等を確認してください。")
endif()

set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(bgfx.cmake)

set(BGFX_TARGETS bgfx bimg bx)
foreach(tgt IN LISTS BGFX_TARGETS)
    if(TARGET ${tgt})
        target_compile_options(${tgt} PRIVATE "-w")
        target_compile_options(${tgt} PRIVATE "-fms-extensions")
    endif()
endforeach()

if(TARGET bgfx AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(bgfx PRIVATE 
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>"
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include bx/string.h>"
    )
endif()

# ターゲット定義と構成

add_executable(BgfxUtl app/main.cpp)

target_link_libraries(BgfxUtl PRIVATE 
    Qt6::Widgets
    Qt6::GuiPrivate
    bgfx
    bx
    bimg
)

if(WIN32 OR LINUX)
    target_link_libraries(BgfxUtl PRIVATE Vulkan::Vulkan)
elseif(APPLE)
    target_link_libraries(BgfxUtl PRIVATE ${METAL_FRAMEWORK} ${QUARTZ_FRAMEWORK})
    
    set_source_files_properties(app/main.cpp PROPERTIES 
        LANGUAGE OBJCXX
    )
endif()
