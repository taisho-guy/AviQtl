cmake_minimum_required(VERSION 3.20)
project(Rina VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler Optimization for Clang
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -fcolor-diagnostics
    )
    # Use LLD or Mold linker if available for faster linking
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -fuse-ld=mold -Wl,--version OUTPUT_VARIABLE MOLD_CHECK ERROR_QUIET)
        if ("${MOLD_CHECK}" MATCHES "mold")
            add_link_options("-fuse-ld=mold")
            message(STATUS "Using mold linker")
        else()
            # add_link_options("-fuse-ld=lld")
        endif()
    endif()
endif()

# Qtの標準最適化を有効化
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

set(EXTRA_QT_COMPONENTS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND EXTRA_QT_COMPONENTS WaylandClient)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Quick3D Widgets Core5Compat Sql Svg Multimedia ${EXTRA_QT_COMPONENTS})
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)

# FFmpeg Integration for Hardware Encoding
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

pkg_check_modules(LUAJIT REQUIRED luajit)

include_directories(
    core/include
    ui/include
    ${LUAJIT_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

# C++ソースコードの収集
file(GLOB_RECURSE CPP_SOURCES 
    "core/src/*.cpp"
    "core/include/*.hpp"
    "engine/*.cpp"
    "engine/*.hpp"
    "ui/src/*.cpp"
    "ui/include/*.hpp"
    "scripting/*.cpp"
    "scripting/*.hpp"
)

# 実行可能ターゲットの作成
qt_add_executable(Rina
    ${CPP_SOURCES}
    ui/resources.qrc
    core/src/settings_manager.cpp
    ui/src/project_service.cpp
    ui/src/transport_service.cpp
    ui/src/selection_service.cpp
    ui/src/clip_model.cpp
    ui/src/timeline_service.cpp
    core/src/video_frame_store.cpp
    core/src/video_frame_provider.cpp
    core/src/video_decoder.cpp
    core/src/project_serializer.cpp
    core/src/video_encoder.cpp
)

# Qt Policy設定: QMLモジュールのディレクトリ構造に関する警告を抑制・解決
qt_policy(SET QTP0004 NEW)

# QMLモジュールの定義
qt_add_qml_module(Rina
    URI "Rina"
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    
    QML_FILES
        ui/qml/MainWindow.qml
        ui/qml/TimelineWindow.qml
        ui/qml/timeline/Ruler.qml
        ui/qml/timeline/LayerHeader.qml
        ui/qml/timeline/TimelineView.qml
        ui/qml/ProjectLauncherWindow.qml
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/SystemSettingsWindow.qml
        ui/qml/SceneSettingsWindow.qml
        ui/qml/CompositeView.qml
        
        ui/qml/common/RinaWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        ui/qml/common/Logger.js
        ui/qml/common/EasingConfigDialog.qml
        ui/qml/common/NodeLoader.qml
        ui/qml/ExportDialog.qml

        # 【削除】Objectsは外部読み込みするため、ここ（コンパイル対象）からは外す
)

# リンク設定
target_link_libraries(
    Rina PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Widgets Qt6::Svg Qt6::Core5Compat Qt6::Multimedia Vulkan::Vulkan
                   ${LUAJIT_LIBRARIES}
                   ${FFMPEG_LIBRARIES})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Rina PRIVATE Qt6::WaylandClient)
endif()

# エフェクト/オブジェクト定義JSONをビルドディレクトリにコピー
file(COPY ui/qml/objects/ DESTINATION ${CMAKE_BINARY_DIR}/objects)
file(COPY ui/qml/effects/ DESTINATION ${CMAKE_BINARY_DIR}/effects)

# エフェクト/オブジェクト定義JSONをビルドディレクトリにコピー
file(COPY ui/qml/objects/ DESTINATION ${CMAKE_BINARY_DIR}/objects)
file(COPY ui/qml/effects/ DESTINATION ${CMAKE_BINARY_DIR}/effects)
