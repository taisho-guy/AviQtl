cmake_minimum_required(VERSION 4.3.3)

project(AviQtlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(UI_SOURCES
    preview/preview.cpp
    settings/settings.cpp
    timeline/timeline.cpp
    resources.qrc
)

set(UI_HEADERS
    preview/preview.hpp
    settings/settings.hpp
    timeline/timeline.hpp
)

add_library(AviQtlUI STATIC ${UI_SOURCES} ${UI_HEADERS})

# UI 層は Qt::Widgets のみに依存。SDL / bgfx の型は見ない。
target_link_libraries(AviQtlUI PRIVATE
    Qt6::Widgets
)

target_include_directories(AviQtlUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
