add_executable(ak_renderprobe
    src/main.cpp
)

target_link_libraries(ak_renderprobe
    PRIVATE
        AK::Core
        AK::Math
        AK::World
        AK::Render
)

set_target_properties(ak_renderprobe PROPERTIES FOLDER "Tools")
