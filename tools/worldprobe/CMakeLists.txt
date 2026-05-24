add_executable(ak_worldprobe
    src/main.cpp
)

target_link_libraries(ak_worldprobe
    PRIVATE
        AK::Core
        AK::Math
        ak_world
)

set_target_properties(ak_worldprobe PROPERTIES FOLDER "Tools")
