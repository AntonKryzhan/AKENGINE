add_executable(ak_boundsprobe
    src/main.cpp
)

target_link_libraries(ak_boundsprobe
    PRIVATE
        AK::Core
        AK::Math
        AK::ECS
        AK::Visibility
)

set_target_properties(ak_boundsprobe PROPERTIES FOLDER "Tools")
