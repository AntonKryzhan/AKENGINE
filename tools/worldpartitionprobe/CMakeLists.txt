add_executable(ak_worldpartitionprobe
    src/main.cpp
)

target_link_libraries(ak_worldpartitionprobe
    PRIVATE
        AK::Core
        AK::WorldPartition
)

set_target_properties(ak_worldpartitionprobe PROPERTIES FOLDER "Tools")
