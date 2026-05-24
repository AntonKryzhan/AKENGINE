add_executable(ak_invariantprobe
    src/main.cpp
)

target_link_libraries(ak_invariantprobe
    PRIVATE
        AK::Core
        AK::ECS
        AK::Assets
        AK::Scene
)

set_target_properties(ak_invariantprobe PROPERTIES FOLDER "Tools")
