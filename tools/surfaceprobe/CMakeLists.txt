add_executable(ak_surfaceprobe
    src/main.cpp
)

target_link_libraries(ak_surfaceprobe
    PRIVATE
        AK::Core
        AK::Surface
)

set_target_properties(ak_surfaceprobe PROPERTIES FOLDER "Tools")
