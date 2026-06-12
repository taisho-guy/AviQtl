cmake_minimum_required(VERSION 4.3.3)

project(AviQtlEngine LANGUAGES CXX)

set(ENGINE_SOURCES
    systems/timeline_evaluation_system.cpp
)

set(ENGINE_HEADERS
    ecs/entity_id.hpp
    ecs/component_storage.hpp
    ecs/world.hpp
    components/scene_component.hpp
    components/time_range_component.hpp
    components/transform_component.hpp
    components/object_kind_component.hpp
    systems/timeline_evaluation_system.hpp
)

add_library(AviQtlEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(AviQtlEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
