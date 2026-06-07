cmake_minimum_required(VERSION 4.3.3)

project(AviQtlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
# Qtリソースコンパイラ（rcc）を有効化
set(CMAKE_AUTORCC ON)

set(UI_SOURCES
    preview/preview.cpp
    settings/settings.cpp
    timeline/timeline.cpp
    # リソースファイルを追加
    resources.qrc
)

set(UI_HEADERS
    preview/preview.hpp
    settings/settings.hpp
    timeline/timeline.hpp
)

add_library(AviQtlUI STATIC ${UI_SOURCES} ${UI_HEADERS})

target_link_libraries(AviQtlUI PRIVATE
    Qt6::Widgets
    Qt6::GuiPrivate
)

target_include_directories(AviQtlUI PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)