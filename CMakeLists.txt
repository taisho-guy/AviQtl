cmake_minimum_required(VERSION 3.20)
project(Rina LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
        -Wno-nullability-extension
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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Quick3D Widgets Core5Compat Sql Svg Multimedia Concurrent ShaderTools LinguistTools Network ${EXTRA_QT_COMPONENTS})
find_package(Vulkan REQUIRED)
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(KF6 REQUIRED COMPONENTS Kirigami ColorScheme)
find_package(PkgConfig REQUIRED)

# FFmpeg Integration for Hardware Encoding
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)


# Carla Audio Plugin Backend
if(WIN32)
    # BUILD.py が生成するパスを優先
    if(NOT CARLA_SDK_DIR)
        set(CARLA_SDK_DIR "${CMAKE_SOURCE_DIR}/carla-sdk")
    endif()

    # Windowsでは手動指定を優先し、なければpkg-configを試す
    find_path(CARLA_INCLUDE_DIR NAMES CarlaBackend.h PATHS ${CARLA_SDK_DIR}/include)
    find_library(CARLA_LIBRARY NAMES carla-standalone PATHS ${CARLA_SDK_DIR}/lib ${CARLA_SDK_DIR})
    
    if(CARLA_INCLUDE_DIR AND CARLA_LIBRARY)
        set(CARLA_FOUND TRUE)
        set(CARLA_INCLUDE_DIRS ${CARLA_INCLUDE_DIR})
        set(CARLA_LIBRARIES ${CARLA_LIBRARY})
        message(STATUS "Found Carla manually: ${CARLA_LIBRARY}")
    endif()
    # If not found manually, try pkg-config
    if(NOT CARLA_FOUND)
        pkg_check_modules(CARLA carla-standalone carla-native-plugin carla-host-plugin carla-utils)
    endif()
else()
    pkg_check_modules(CARLA REQUIRED carla-standalone carla-native-plugin carla-host-plugin carla-utils)
endif()

if(NOT CARLA_FOUND AND NOT CARLA_LIBRARIES) # Check both for robustness
    message(FATAL_ERROR "Carla not found. Ensure it's installed (e.g., via Homebrew on macOS, or CARLA_SDK_DIR is set on Windows).")
endif()

pkg_check_modules(LUAJIT REQUIRED luajit)

# macOSでのLuaJITリンク問題の修正
if(APPLE)
    # pkg-configが返す-lluajit-5.1を-lluajitに変更、またはHomebrewの絶対パスを使用
    string(REPLACE "-lluajit-5.1" "-lluajit" LUAJIT_LIBRARIES "${LUAJIT_LIBRARIES}")
endif()

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

# QML/JSファイルの収集 (lupdateのスキャン対象にするため)
file(GLOB_RECURSE QML_SOURCES 
    "ui/qml/*.qml"
    "ui/qml/*.js"
)

# 共通シェーダーライブラリ (依存関係用)
file(GLOB_RECURSE COMMON_SHADERS "ui/qml/common/shaders/*.glsl")

# 実行可能ターゲットの作成
qt_add_executable(Rina
    ${CPP_SOURCES}
    core/src/settings_manager.cpp
    core/src/color_scheme_controller.cpp
    ui/src/project_service.cpp
    ui/src/transport_service.cpp
    ui/src/selection_service.cpp
    ui/src/clip_model.cpp
    ui/src/timeline_service.cpp
    ui/src/timeline_media_manager.cpp
    ui/src/timeline_engine_synchronizer.cpp
    ui/src/timeline_export_manager.cpp
    core/src/video_frame_store.cpp
    core/src/video_frame_provider.cpp
    core/src/video_decoder.cpp
    core/src/image_decoder.cpp
    core/src/audio_decoder.cpp
    engine/audio_mixer.cpp
    core/src/media_decoder.cpp
    core/src/project_serializer.cpp
    core/src/video_encoder.cpp
    scripting/mod_engine.cpp
    ${COMMON_SHADERS}
    ${QML_SOURCES}
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
        ui/qml/SceneRenderer.qml
        ui/qml/timeline/Ruler.qml
        ui/qml/timeline/LayerHeader.qml
        ui/qml/timeline/TimelineView.qml
        ui/qml/timeline/ClipItem.qml
        ui/qml/timeline/TimelineGrid.qml
        ui/qml/timeline/GridSettingsWindow.qml
        ui/qml/ProjectLauncherWindow.qml
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/SystemSettingsWindow.qml
        ui/qml/SceneSettingsWindow.qml
        ui/qml/CompositeView.qml
        ui/qml/ExportDialog.qml
        ui/qml/AboutWindow.qml
        ui/qml/UpdateNotificationWindow.qml
        
        ui/qml/common/RinaWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ControlLoader.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        ui/qml/common/Logger.js
        ui/qml/common/EasingConfigWindow.qml
        ui/qml/common/NodeLoader.qml
        ui/qml/common/Icons.js
        ui/qml/common/RinaIcon.qml
        ui/qml/common/IconMenuItem.qml
        ui/qml/common/AudioPluginMenu.qml

    RESOURCES
        ui/resources.qrc
)

file(GLOB RINA_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/i18n/Rina_*.ts")

if(RINA_TS_FILES)
    qt_add_translations(Rina
        TS_FILES ${RINA_TS_FILES}
    )
    # ビルド時に抽出(lupdate)とバイナリ変換(lrelease)を両方実行させる
    add_dependencies(Rina Rina_lupdate Rina_lrelease)
endif()

# リンク設定
target_link_libraries(
    Rina PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Widgets Qt6::Svg Qt6::Core5Compat Qt6::Multimedia Qt6::Concurrent Qt6::Network Vulkan::Vulkan
                   ${LUAJIT_LIBRARIES}
                   ${FFMPEG_LIBRARIES}
                   ${CARLA_LIBRARIES}
                   KF6::Kirigami KF6::ColorScheme)


# Carlaのライブラリパスを明示（/usr/lib/carla/ 等の非標準ディレクトリ対策）
target_link_directories(Rina PRIVATE ${CARLA_LIBRARY_DIRS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Rina PRIVATE Qt6::WaylandClient)
endif()

# ターゲットが定義された後にインクルードを追加
target_include_directories(Rina PRIVATE
    core/include
    ui/include
    ${LUAJIT_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${CARLA_INCLUDE_DIRS}
)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_compile_options(Rina PRIVATE
        -Wno-deprecated-declarations
        -Wno-non-virtual-dtor # TODO investigate this!
    )
endif()

# QMLオブジェクト/エフェクト/プラグインをビルド時に毎回コピー
add_custom_target(copy_runtime_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/ui/qml/objects ${CMAKE_BINARY_DIR}/objects
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/ui/qml/effects ${CMAKE_BINARY_DIR}/effects
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/plugins        ${CMAKE_BINARY_DIR}/plugins
    COMMENT "Runtime assets をビルドディレクトリへコピー"
    VERBATIM
)

# シェーダーのコンパイル (.frag -> .qsb)
function(compile_shaders TARGET_SUFFIX SRC_DIR DEST_DIR)
    file(GLOB SHADERS "${SRC_DIR}/*.frag" "${SRC_DIR}/*.vert")
    set(QSB_OUTPUTS "")
    foreach(SHADER ${SHADERS})
        get_filename_component(FILENAME ${SHADER} NAME)
        set(QSB_FILE "${DEST_DIR}/${FILENAME}.qsb")
        
        add_custom_command(
            OUTPUT ${QSB_FILE}
            COMMAND Qt6::qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 ${SHADER} -o ${QSB_FILE}
            DEPENDS ${SHADER} ${COMMON_SHADERS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Compiling shader ${FILENAME} to .qsb"
        )
        list(APPEND QSB_OUTPUTS ${QSB_FILE})
    endforeach()
    
    if(QSB_OUTPUTS)
        add_custom_target(CompileShaders_${TARGET_SUFFIX} ALL DEPENDS ${QSB_OUTPUTS})
        add_dependencies(Rina CompileShaders_${TARGET_SUFFIX})
    endif()
endfunction()

compile_shaders("Effects" "ui/qml/effects" "${CMAKE_BINARY_DIR}/effects")
compile_shaders("Objects" "ui/qml/objects" "${CMAKE_BINARY_DIR}/objects")
