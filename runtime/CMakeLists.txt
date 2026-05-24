add_executable(ak_player
    src/main.cpp
)

target_link_libraries(ak_player
    PRIVATE
        AK::Core
        AK::Platform
        AK::Scene
        AK::Render
)

set_target_properties(ak_player PROPERTIES FOLDER "Applications")
