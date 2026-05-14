cmake_minimum_required(VERSION 3.25)
project(AviQtl LANGUAGES CXX C)

# 実行ファイルの出力先を bin/ に設定
# qt_add_qml_module(AviQtl) が同名ディレクトリを生成するため衝突を回避
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt の Private モジュール使用に関する警告を抑制
set(QT_NO_PRIVATE_MODULE_WARNING ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qtの標準最適化を有効化

set(EXTRA_QT_COMPONENTS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND EXTRA_QT_COMPONENTS WaylandClient)
endif()


# --- Filament Local Static Build Setup ---
set(FILAMENT_LOCAL_DIR "${CMAKE_SOURCE_DIR}/vendor/filament")
if(NOT EXISTS "${FILAMENT_LOCAL_DIR}")
    message(FATAL_ERROR "Filament binaries not found in vendor/filament.")
endif()

include_directories(SYSTEM "${FILAMENT_LOCAL_DIR}/include")
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FILAMENT_MSVC_RUNTIME_DIR "mdd")
    else()
        set(FILAMENT_MSVC_RUNTIME_DIR "md")
    endif()
    set(FILAMENT_LIB_DIR "${FILAMENT_LOCAL_DIR}/lib/x86_64/${FILAMENT_MSVC_RUNTIME_DIR}")
elseif(APPLE AND (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
    set(FILAMENT_LIB_DIR "${FILAMENT_LOCAL_DIR}/lib/arm64")
else()
    set(FILAMENT_LIB_DIR "${FILAMENT_LOCAL_DIR}/lib/x86_64")
endif()

link_directories("${FILAMENT_LIB_DIR}")

set(FILAMENT_LIBS filament backend bluevk bluegl filabridge filaflat utils geometry smol-v zstd)
if(NOT WIN32)
    list(APPEND FILAMENT_LIBS pthread dl)
endif()
set(FILAMENT_TOOLS_DIR "${FILAMENT_LOCAL_DIR}/bin")

find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate Quick QuickControls2 Widgets Core5Compat Sql Svg Multimedia Concurrent ShaderTools LinguistTools Network ${EXTRA_QT_COMPONENTS})
if(APPLE)
    find_package(Vulkan)
else()
    find_package(Vulkan REQUIRED)
endif()
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG IMPORTED_TARGET REQUIRED libavcodec libavformat libavutil libswscale libswresample)

# Carla Audio Plugin Backend
if(WIN32)
    # BUILD.py が生成するパスを優先
    if(NOT CARLA_SDK_DIR)
        set(CARLA_SDK_DIR "${CMAKE_SOURCE_DIR}/vendor/carla")
    endif()

    # Windows では Carla 公式バイナリに pkg-config が含まれないため手動指定を優先
    find_path(CARLA_INCLUDE_DIR NAMES CarlaBackend.h PATHS ${CARLA_SDK_DIR}/include)
    find_library(CARLA_STANDALONE_LIBRARY NAMES libcarla_standalone2 carla_standalone2 carla-standalone PATHS ${CARLA_SDK_DIR}/lib ${CARLA_SDK_DIR})
    find_library(CARLA_NATIVE_PLUGIN_LIBRARY NAMES libcarla_native-plugin carla_native-plugin carla-native-plugin PATHS ${CARLA_SDK_DIR}/lib ${CARLA_SDK_DIR})
    find_library(CARLA_HOST_PLUGIN_LIBRARY NAMES libcarla_host-plugin carla_host-plugin carla-host-plugin PATHS ${CARLA_SDK_DIR}/lib ${CARLA_SDK_DIR})
    find_library(CARLA_UTILS_LIBRARY NAMES libcarla_utils carla_utils carla-utils PATHS ${CARLA_SDK_DIR}/lib ${CARLA_SDK_DIR})

    if(CARLA_INCLUDE_DIR AND CARLA_STANDALONE_LIBRARY AND CARLA_NATIVE_PLUGIN_LIBRARY AND CARLA_HOST_PLUGIN_LIBRARY AND CARLA_UTILS_LIBRARY)
        set(CARLA_FOUND TRUE)
        set(CARLA_INCLUDE_DIRS ${CARLA_INCLUDE_DIR})
        set(CARLA_LIBRARY_DIRS ${CARLA_SDK_DIR}/lib)
        set(CARLA_LIBRARIES
            ${CARLA_STANDALONE_LIBRARY}
            ${CARLA_NATIVE_PLUGIN_LIBRARY}
            ${CARLA_HOST_PLUGIN_LIBRARY}
            ${CARLA_UTILS_LIBRARY}
        )
        message(STATUS "Found Carla manually: ${CARLA_LIBRARIES}")
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

pkg_check_modules(LUAJIT IMPORTED_TARGET REQUIRED luajit)
# macOSでHomebrewのライブラリパスを明示的に追加

if(APPLE)
    link_directories(/opt/homebrew/lib)
endif()

# C++ソースコードの収集
file(GLOB_RECURSE CPP_SOURCES
    CONFIGURE_DEPENDS
    "core/src/*.cpp"
    "core/include/*.hpp"
    "engine/*.cpp"
    "engine/*.hpp"
    "rendering/src/*.cpp"
    "rendering/include/*.hpp"
    "ui/src/*.cpp"
    "ui/include/*.hpp"
    "scripting/*.cpp"
    "scripting/*.hpp"
)

# QML/JSファイルの収集 (lupdateのスキャン対象にするため)
file(GLOB_RECURSE QML_SOURCES
    CONFIGURE_DEPENDS
    "ui/qml/*.qml"
    "ui/qml/*.js"
)

# 共通シェーダーライブラリ (依存関係用)
file(GLOB_RECURSE COMMON_SHADERS "ui/qml/common/shaders/*.glsl")

# 実行可能ターゲットの作成
qt_add_executable(AviQtl
    ${CPP_SOURCES}
    core/src/settings_manager.cpp
    ui/src/project_service.cpp
    ui/src/transport_service.cpp
    ui/src/selection_service.cpp
    ui/src/clip_model.cpp
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
)

if(APPLE)
    set_target_properties(AviQtl PROPERTIES MACOSX_BUNDLE ON)
    enable_language(OBJCXX)
    set_source_files_properties(rendering/src/filament_canvas.cpp PROPERTIES LANGUAGE OBJCXX)
endif()

# Phase 2.1: シンボルのエクスポート設定 (Lua FFI用)
set_target_properties(AviQtl PROPERTIES ENABLE_EXPORTS ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(AviQtl PRIVATE "-rdynamic")
endif()

# Vulkan のバージョン衝突回避と Filament/BlueVK の要件
target_compile_definitions(AviQtl PRIVATE 
    VK_NO_PROTOTYPES
)

# Qt Policy設定: QMLモジュールのディレクトリ構造に関する警告を抑制・解決
qt_policy(SET QTP0004 NEW)

# QMLモジュールの定義
qt_add_qml_module(AviQtl
    URI "AviQtl"
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"

    # QML_ELEMENT を持つ型のヘッダのみ指定する (.cpp は AviQtl 本体でコンパイル済み)
    # filament_canvas / core_bridge は QML_ELEMENT なし → main.cpp の qmlRegisterType で登録
    SOURCES
        core/include/compute_effect.hpp

    QML_FILES
        ui/qml/MainWindow.qml
        ui/qml/TimelineWindow.qml
        ui/qml/SceneRenderer.qml
        ui/qml/timeline/Ruler.qml
        ui/qml/timeline/LayerHeader.qml
        ui/qml/timeline/TimelineView.qml
        ui/qml/timeline/ClipItem.qml
        ui/qml/timeline/TimelineGrid.qml
            ui/qml/ProjectLauncherWindow.qml
        ui/qml/ProjectSettingsWindow.qml
        ui/qml/SettingDialog.qml
        ui/qml/SystemSettingsWindow.qml
        ui/qml/settings/GeneralSettingsPage.qml
        ui/qml/settings/PerformanceSettingsPage.qml
        ui/qml/settings/TimelineSettingsPage.qml
        ui/qml/settings/AppearanceSettingsPage.qml
        ui/qml/settings/ProjectSettingsPage.qml
        ui/qml/settings/ExportSettingsPage.qml
        ui/qml/settings/DecodeAudioSettingsPage.qml
        ui/qml/settings/PluginSettingsPage.qml
        ui/qml/settings/ShortcutSettingsPage.qml
        ui/qml/SceneSettingsWindow.qml
        ui/qml/CompositeView.qml
        ui/qml/ExportDialog.qml
        ui/qml/AboutWindow.qml
        ui/qml/PackageManagerWindow.qml

        ui/qml/common/AviQtlWindow.qml
        ui/qml/common/ParamControl.qml
        ui/qml/common/ControlLoader.qml
        ui/qml/common/ObjectRenderer.qml
        ui/qml/common/BaseObject.qml
        ui/qml/common/BaseEffect.qml
        ui/qml/common/BaseComputeEffect.qml
        ui/qml/common/Logger.js
        ui/qml/common/EasingConfigWindow.qml
        ui/qml/common/NodeLoader.qml
        ui/qml/common/Icons.js
        ui/qml/common/AviQtlIcon.qml
        ui/qml/common/IconMenuItem.qml
        ui/qml/common/AudioPluginMenu.qml

    RESOURCES
        ui/resources.qrc
)

file(GLOB AVIQTL_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/i18n/AviQtl_*.ts")

if(AVIQTL_TS_FILES)
    qt_add_translations(AviQtl
        TS_FILES ${AVIQTL_TS_FILES}
    )
    # ビルド時に抽出(lupdate)とバイナリ変換(lrelease)を両方実行させる
    add_dependencies(AviQtl AviQtl_lupdate AviQtl_lrelease)
endif()

# リンク設定
target_link_libraries(
    AviQtl PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Svg Qt6::Core5Compat Qt6::Multimedia Qt6::Concurrent Qt6::Network
                   PkgConfig::LUAJIT
                   PkgConfig::FFMPEG
                   ${CARLA_LIBRARIES}
)

# VulkanはLinux/Windowsで必須。macOSではMetalを使用するためオプション
if(NOT APPLE)
    target_link_libraries(AviQtl PRIVATE Vulkan::Vulkan)
elseif(Vulkan_FOUND)
    target_link_libraries(AviQtl PRIVATE Vulkan::Vulkan)
endif()

# Carlaのライブラリパスを明示（/usr/lib/carla/ 等の非標準ディレクトリ対策）
target_link_directories(AviQtl PRIVATE ${CARLA_LIBRARY_DIRS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(AviQtl PRIVATE ${FILAMENT_LIBS} Qt6::WaylandClient
        # vendor/filament は libc++ (LLVM ABI) でビルドされているため c++ が必須
        c++
        c++abi
        # Qt6 は libstdc++ を参照するため stdc++ も残す
        stdc++
        # carla が間接依存する fluidsynth を --as-needed 対策で明示リンク
        fluidsynth
    )
elseif(APPLE)
    target_link_libraries(AviQtl PRIVATE ${FILAMENT_LIBS}
        "-framework CoreVideo"
        "-framework QuartzCore"
    )
elseif(MSVC)
    target_link_libraries(AviQtl PRIVATE ${FILAMENT_LIBS} opengl32)
endif()

# ターゲットが定義された後にインクルードを追加
target_include_directories(AviQtl PRIVATE
    # ルートを追加: "engine/timeline/ecs.hpp" 等のルート相対 #include を解決
    ${CMAKE_SOURCE_DIR}
    core/include
    ui/include
    rendering/include
    ${PROJECT_SOURCE_DIR}/vendor/filament/include/backend
    ${CARLA_INCLUDE_DIRS}
)

# コンパイルオプション (ジェネレータ式で条件分岐を統合)
target_compile_features(AviQtl PRIVATE cxx_std_23)
if(MSVC)
    set_target_properties(AviQtl PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
target_compile_options(AviQtl PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -O3
        -flto
        -fno-semantic-interposition
    >
    $<$<CXX_COMPILER_ID:Clang>:
        -fcolor-diagnostics
        -Wno-nullability-extension
        -flto=thin
        -Wno-deprecated-declarations
        -Wno-non-virtual-dtor
        -fno-direct-access-external-data
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /permissive-
        /utf-8
    >
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
        /GL
    >
)
target_link_options(AviQtl PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Linux>>:
        -flto=thin
    >
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
        /LTCG
    >
)

# リンカ選択 (CMAKE_LINKER_TYPE: CMake 3.29+)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DEFINED CMAKE_LINKER_TYPE)
    find_program(MOLD_LINKER mold)
    find_program(LLD_LINKER  NAMES ld.lld lld)
    if(MOLD_LINKER)
        set(CMAKE_LINKER_TYPE MOLD CACHE STRING "" FORCE)
    elseif(LLD_LINKER)
        set(CMAKE_LINKER_TYPE LLD  CACHE STRING "" FORCE)
    else()
        message(WARNING "mold/lld not found. ThinLTO may be disabled.")
    endif()
endif()

# QMLオブジェクト/エフェクト/プラグインをビルド時に毎回コピー
file(WRITE ${CMAKE_BINARY_DIR}/copy_assets.cmake "
file(COPY \"${CMAKE_SOURCE_DIR}/ui/qml/objects/\" DESTINATION \"${CMAKE_BINARY_DIR}/objects\" PATTERN \"*.frag\" EXCLUDE PATTERN \"*.vert\" EXCLUDE PATTERN \"*.comp\" EXCLUDE PATTERN \"*.glsl\" EXCLUDE)
file(COPY \"${CMAKE_SOURCE_DIR}/ui/qml/effects/\" DESTINATION \"${CMAKE_BINARY_DIR}/effects\" PATTERN \"*.frag\" EXCLUDE PATTERN \"*.vert\" EXCLUDE PATTERN \"*.comp\" EXCLUDE PATTERN \"*.glsl\" EXCLUDE)
file(COPY \"${CMAKE_SOURCE_DIR}/plugins/\" DESTINATION \"${CMAKE_BINARY_DIR}/plugins\")
")
add_custom_target(copy_runtime_assets ALL
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/copy_assets.cmake
    COMMENT "Copying runtime assets (excluding shader sources)"
    VERBATIM
)

# シェーダーのコンパイル (.frag/.vert/.comp -> .qsb)
# qt6_add_shaders の標準機能を活用して .comp を .frag/.vert と同列にコンパイルする。
# PREFIX と BASE を指定し、リソース /qt/qml/AviQtl/effects および objects 以下に
# コンパイル済みの .qsb を直接展開する。これによって、QMLからローカルの相対パス
# （fragmentShader: "borderblur.frag.qsb" 等）でシームレスに参照可能となる。

file(GLOB EFFECT_SHADERS_FRAGVERT "ui/qml/effects/*.frag" "ui/qml/effects/*.vert")
file(GLOB EFFECT_SHADERS_COMP "ui/qml/effects/*.comp")
file(GLOB OBJECT_SHADERS_FRAGVERT "ui/qml/objects/*.frag" "ui/qml/objects/*.vert")
file(GLOB OBJECT_SHADERS_COMP "ui/qml/objects/*.comp")

qt6_add_shaders(AviQtl "aviqtl_effects_fragvert"
    GLSL "100 es,120,150,310 es,430"
    HLSL 50
    MSL 20
    BATCHABLE
    PRECOMPILE
    OPTIMIZED
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
    BASE "ui/qml"
    FILES ${EFFECT_SHADERS_FRAGVERT}
)

if(EFFECT_SHADERS_COMP)
    qt6_add_shaders(AviQtl "aviqtl_effects_comp"
        GLSL "310 es,430"
        HLSL 50
        MSL 20
        BATCHABLE
        PRECOMPILE
        OPTIMIZED
        OUTPUT_DIR "${CMAKE_BINARY_DIR}"
        BASE "ui/qml"
        FILES ${EFFECT_SHADERS_COMP}
    )
endif()

qt6_add_shaders(AviQtl "aviqtl_objects_fragvert"
    GLSL "100 es,120,150,310 es,430"
    HLSL 50
    MSL 20
    BATCHABLE
    PRECOMPILE
    OPTIMIZED
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
    BASE "ui/qml"
    FILES ${OBJECT_SHADERS_FRAGVERT}
)

if(OBJECT_SHADERS_COMP)
    qt6_add_shaders(AviQtl "aviqtl_objects_comp"
        GLSL "310 es,430"
        HLSL 50
        MSL 20
        BATCHABLE
        PRECOMPILE
        OPTIMIZED
        OUTPUT_DIR "${CMAKE_BINARY_DIR}"
        BASE "ui/qml"
        FILES ${OBJECT_SHADERS_COMP}
    )
endif()

# Trigger rescan for workspace.cpp and workspace.hpp
