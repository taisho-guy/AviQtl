cmake_minimum_required(VERSION 4.3.3)

project(AviQtlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# [変更] preview は system 層の PreviewHostSdl に統合したため削除。
#        UI 層は settings / timeline のみ。
set(UI_SOURCES
    settings/settings.cpp
    timeline/timeline.cpp
    resources.qrc
)

set(UI_HEADERS
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
