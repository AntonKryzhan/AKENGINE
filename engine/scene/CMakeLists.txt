add_library(ak_scene STATIC
    src/Scene.cpp
)

add_library(AK::Scene ALIAS ak_scene)

target_include_directories(ak_scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_scene
    PUBLIC
        AK::Core
        AK::ECS
)

set_target_properties(ak_scene PROPERTIES FOLDER "Engine")
