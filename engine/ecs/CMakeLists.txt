add_library(ak_ecs STATIC
    src/EntityId.cpp
    src/World.cpp
)

add_library(AK::ECS ALIAS ak_ecs)

target_include_directories(ak_ecs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_ecs
    PUBLIC
        AK::Core
        AK::Math
        AK::World
)

set_target_properties(ak_ecs PROPERTIES FOLDER "Engine")
