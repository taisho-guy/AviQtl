# --- Test Infrastructure ---
# Each subdirectory under tests/ corresponds to a test category.
# They are linked as standalone executables and registered with CTest.

# --- Dependencies ---
find_package(Qt6 REQUIRED COMPONENTS Test)

if(POLICY CMP0110)
    cmake_policy(SET CMP0110 NEW)
endif()

# --- Helper: Add a single test target ---
# Usage:
#   aviqtl_add_test(NAME effect_registry
#       SOURCES test_effect_registry.cpp
#       LINKS AviQtl_Core
#   )
function(aviqtl_add_test)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LINKS)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TARGET_NAME "AviQtl_Test_${ARG_NAME}")
    add_executable(${TARGET_NAME} ${ARG_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Test)
    if(ARG_LINKS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_LINKS})
    endif()

    # Register with CTest
    add_test(NAME ${ARG_NAME} COMMAND ${TARGET_NAME})

    # On Windows, ensure vendor DLLs (e.g., Carla) are in PATH for runtime loading.
    # ENVIRONMENT_MODIFICATION handles platform-specific path separators correctly.
    if(WIN32)
        set_tests_properties(${ARG_NAME} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_SOURCE_DIR}/vendor/carla/lib"
        )
    endif()
endfunction()

# --- Core Tests ---
aviqtl_add_test(
    NAME effect_registry
    SOURCES test_effect_registry.cpp
    LINKS AviQtl_Core
)

aviqtl_add_test(
    NAME document_model
    SOURCES test_document_model.cpp
    LINKS AviQtl_Core
)

aviqtl_add_test(
    NAME settings_manager
    SOURCES test_settings_manager.cpp
    LINKS AviQtl_Core
)

aviqtl_add_test(
    NAME theme_controller
    SOURCES test_theme_controller.cpp
    LINKS AviQtl_Core
)

# --- Engine Tests ---
aviqtl_add_test(
    NAME dense_component_map
    SOURCES test_dense_component_map.cpp
    LINKS AviQtl_Engine
)

aviqtl_add_test(
    NAME audio_plugin_chain
    SOURCES test_audio_plugin_chain.cpp
    LINKS AviQtl_Engine
)

# --- Scripting Tests ---
aviqtl_add_test(
    NAME lua_host
    SOURCES test_lua_host.cpp
    LINKS AviQtl_Scripting
)

# --- UI Tests ---
aviqtl_add_test(
    NAME selection_service
    SOURCES test_selection_service.cpp
    LINKS AviQtl_UI
)

aviqtl_add_test(
    NAME project_service
    SOURCES test_project_service.cpp
    LINKS AviQtl_UI
)
