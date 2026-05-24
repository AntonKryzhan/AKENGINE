add_executable(ak_worldtopologyprobe
    src/main.cpp
)

target_link_libraries(ak_worldtopologyprobe
    PRIVATE
        AK::Core
        AK::WorldTopology
)

set_target_properties(ak_worldtopologyprobe PROPERTIES FOLDER "Tools")
