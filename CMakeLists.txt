cmake_minimum_required(VERSION 3.20)
project(Rina VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON) # QtのMOCを自動実行
set(CMAKE_AUTORCC ON) # Qtのリソースコンパイラを自動実行

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 WaylandClient Quick3D)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

include_directories(
    core/include
    ui/include
    ${LUAJIT_INCLUDE_DIRS}
)

# ヘッダーファイルも明示的にソースリストに含める (AUTOMOCのため)
file(GLOB_RECURSE SOURCES 
    "core/src/*.cpp"
    "core/include/*.hpp"
    "engine/*.cpp"
    "engine/*.hpp"
    "ui/src/*.cpp"
    "ui/include/*.hpp"
    "scripting/*.cpp"
    "scripting/*.hpp"
)

add_executable(Rina ${SOURCES} ui/resources.qrc)

target_link_libraries(Rina PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::WaylandClient Qt6::Quick3D
    Vulkan::Vulkan
    ${LUAJIT_LIBRARIES}
)
