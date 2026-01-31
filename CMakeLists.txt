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
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -fuse-ld=mold -Wl,--version OUTPUT_VARIABLE MOLD_CHECK ERROR_QUIET)
    if ("${MOLD_CHECK}" MATCHES "mold")
        add_link_options("-fuse-ld=mold")
        message(STATUS "Using mold linker")
    else()
        add_link_options("-fuse-ld=lld")
    endif()
endif()

# Qtの標準最適化を有効化
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 WaylandClient Quick3D Widgets Core5Compat Sql Svg)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

include_directories(
    core/include
    ui/include
    ${LUAJIT_INCLUDE_DIRS}
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
    core/src/project_serializer.cpp
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
        ui/qml/ProjectLauncherWindow.qml
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/SystemSettingsWindow.qml
        ui/qml/CompositeView.qml
        
        ui/qml/common/RinaWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        
        # 拡張オブジェクト・エフェクトは外部ロードするため除外
)

# 拡張リソース（エフェクト、オブジェクト）を出力ディレクトリへコピー
file(GLOB_RECURSE EFFECT_FILES "ui/qml/effects/*")
file(GLOB_RECURSE OBJECT_FILES "ui/qml/objects/*")

add_custom_command(TARGET Rina POST_BUILD
    # Effectsディレクトリ
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:Rina>/effects
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Rina>/effects
    COMMAND ${CMAKE_COMMAND} -E copy ${EFFECT_FILES} $<TARGET_FILE_DIR:Rina>/effects
    # Objectsディレクトリ（QMLとJSON両方をコピー）
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:Rina>/objects
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Rina>/objects
    COMMAND ${CMAKE_COMMAND} -E copy ${OBJECT_FILES} $<TARGET_FILE_DIR:Rina>/objects
    COMMENT "Deploying extensible effects and objects..."
)

# リンク設定
target_link_libraries(Rina PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::WaylandClient Qt6::Quick3D
    Qt6::Widgets
    Qt6::Svg
    Qt6::Core5Compat
    Vulkan::Vulkan
    ${LUAJIT_LIBRARIES}
)
