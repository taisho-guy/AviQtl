cmake_minimum_required(VERSION 3.20)
project(Rina VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qtの標準最適化を有効化
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 WaylandClient Quick3D Widgets Core5Compat Sql)
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
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/CompositeView.qml
        
        ui/qml/common/RinaWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        
        # 拡張オブジェクト・エフェクトは外部ロードするため除外
    
    RESOURCES
        assets/icon.png
)

# 拡張リソース（エフェクト、オブジェクト）を出力ディレクトリへコピー
file(GLOB_RECURSE EFFECT_FILES "ui/qml/effects/*")
file(GLOB_RECURSE OBJECT_FILES "ui/qml/objects/*")

add_custom_command(TARGET Rina POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:Rina>/effects
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Rina>/effects
    COMMAND ${CMAKE_COMMAND} -E copy ${EFFECT_FILES} $<TARGET_FILE_DIR:Rina>/effects
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:Rina>/objects
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Rina>/objects
    COMMAND ${CMAKE_COMMAND} -E copy ${OBJECT_FILES} $<TARGET_FILE_DIR:Rina>/objects
    COMMENT "Deploying extensible effects and objects..."
)

# リンク設定
target_link_libraries(Rina PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::WaylandClient Qt6::Quick3D
    Qt6::Widgets
    Qt6::Core5Compat
    Vulkan::Vulkan
    ${LUAJIT_LIBRARIES}
)
