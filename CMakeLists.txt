cmake_minimum_required(VERSION 3.20)
project(Rina VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler Optimization
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -O3
        -flto
        -fno-semantic-interposition
    )
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    add_compile_options(
        -fcolor-diagnostics
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_program(MOLD_LINKER mold)
        find_program(LLD_LINKER NAMES ld.lld lld)
        if(MOLD_LINKER)
            add_link_options("-fuse-ld=${MOLD_LINKER}")
            add_compile_options("-flto=thin")
            add_link_options("-flto=thin")
        elseif(LLD_LINKER)
            add_link_options("-fuse-ld=${LLD_LINKER}")
            add_compile_options("-flto=thin")
            add_link_options("-flto=thin")
        else()
            message(WARNING "Mold or LLD linker not found. ThinLTO will be disabled to prevent link errors.")
        endif()
    else()
        add_compile_options("-flto=thin")
    endif()
endif()

# Qtの標準最適化を有効化
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

set(EXTRA_QT_COMPONENTS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND EXTRA_QT_COMPONENTS WaylandClient)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Quick3D Widgets Core5Compat Sql Svg Multimedia Concurrent ${EXTRA_QT_COMPONENTS})
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)

# FFmpeg Integration for Hardware Encoding
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)

pkg_check_modules(LUAJIT REQUIRED luajit)

# オーディオプラグイン (ヘッダー探索のみ、リンクは後で行う)
find_path(LADSPA_INCLUDE_DIR ladspa.h)
pkg_check_modules(LILV REQUIRED lilv-0)
find_path(CLAP_INCLUDE_DIR  clap/clap.h)
find_path(VST3SDK_INCLUDE_DIR pluginterfaces/vst/ivstaudioprocessor.h
    HINTS /usr/include/vst3sdk)

if(CLAP_INCLUDE_DIR)
    message(STATUS "Found CLAP headers: ${CLAP_INCLUDE_DIR}")
else()
    message(WARNING "clap headers not found. CLAP support disabled.")
endif()
if(NOT VST3SDK_INCLUDE_DIR)
    message(STATUS "VST3 SDK not found. VST3 will be stub-only.")
endif()

# macOSでのLuaJITリンク問題の修正
if(APPLE)
    # pkg-configが返す-lluajit-5.1を-lluajitに変更、またはHomebrewの絶対パスを使用
    string(REPLACE "-lluajit-5.1" "-lluajit" LUAJIT_LIBRARIES "${LUAJIT_LIBRARIES}")
endif()

include_directories(
    core/include
    ui/include
    ${LUAJIT_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

# macOSでHomebrewのライブラリパスを明示的に追加
if(APPLE)
    link_directories(/opt/homebrew/lib)
endif()

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
    core/src/audio_decoder.cpp
    engine/audio_mixer.cpp
    core/src/project_serializer.cpp
    core/src/video_encoder.cpp
    scripting/mod_engine.cpp
)

if(APPLE)
    set_target_properties(Rina PROPERTIES MACOSX_BUNDLE ON)
endif()

# Phase 2.1: シンボルのエクスポート設定 (Lua FFI用)
set_target_properties(Rina PROPERTIES ENABLE_EXPORTS ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(Rina PRIVATE "-rdynamic")
endif()

# Qt Policy設定: QMLモジュールのディレクトリ構造に関する警告を抑制・解決
if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

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
        ui/qml/timeline/GridSettingsWindow.qml
        ui/qml/ProjectLauncherWindow.qml
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/SystemSettingsWindow.qml
        ui/qml/SceneSettingsWindow.qml
        ui/qml/CompositeView.qml
        
        ui/qml/common/RinaWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ControlLoader.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        ui/qml/common/Logger.js
        ui/qml/common/EasingConfigDialog.qml
        ui/qml/common/NodeLoader.qml
        ui/qml/common/Icons.js
        ui/qml/common/RinaIcon.qml
        ui/qml/common/IconMenuItem.qml
        ui/qml/ExportDialog.qml

        # 【削除】Objectsは外部読み込みするため、ここ（コンパイル対象）からは外す
)

# リンク設定
target_link_libraries(
    Rina PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Widgets Qt6::Svg Qt6::Core5Compat Qt6::Multimedia Qt6::Concurrent Vulkan::Vulkan
                   ${LUAJIT_LIBRARIES}
                   ${FFMPEG_LIBRARIES}
                   ${LILV_LIBRARIES})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Rina PRIVATE Qt6::WaylandClient)
endif()

# ターゲットが定義された後にインクルードを追加
target_include_directories(Rina PRIVATE
    ${LADSPA_INCLUDE_DIR}
    ${LILV_INCLUDE_DIRS}
)
if(CLAP_INCLUDE_DIR)
    target_include_directories(Rina PRIVATE ${CLAP_INCLUDE_DIR})
    target_compile_definitions(Rina PRIVATE RINA_HAS_CLAP=1)
else()
    target_compile_definitions(Rina PRIVATE RINA_HAS_CLAP=0)
endif()

if(VST3SDK_INCLUDE_DIR)
    # pluginterfaces/ の親ディレクトリを追加
    get_filename_component(VST3SDK_ROOT "${VST3SDK_INCLUDE_DIR}" DIRECTORY)
    target_include_directories(Rina PRIVATE ${VST3SDK_ROOT})
endif()

# エフェクト/オブジェクト定義JSONをビルドディレクトリにコピー
file(COPY ui/qml/objects/ DESTINATION ${CMAKE_BINARY_DIR}/objects)
file(COPY ui/qml/effects/ DESTINATION ${CMAKE_BINARY_DIR}/effects)
file(COPY plugins/ DESTINATION ${CMAKE_BINARY_DIR}/plugins)
