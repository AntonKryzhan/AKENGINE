add_executable(ak_assetdbprobe
    src/main.cpp
)

target_link_libraries(ak_assetdbprobe
    PRIVATE
        AK::Assets
)

set_target_properties(ak_assetdbprobe PROPERTIES FOLDER "Tools")
