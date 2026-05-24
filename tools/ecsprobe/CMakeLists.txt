add_executable(ak_ecsprobe
    src/main.cpp
)

target_link_libraries(ak_ecsprobe
    PRIVATE
        AK::Core
        AK::ECS
        AK::Scene
)

set_target_properties(ak_ecsprobe PROPERTIES FOLDER "Tools")
