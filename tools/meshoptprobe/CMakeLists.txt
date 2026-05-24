add_executable(ak_meshoptprobe
    src/main.cpp
)

target_link_libraries(ak_meshoptprobe
    PRIVATE
        AK::Core
        AK::Math
        AK::RHI
        AK::Render
)

set_target_properties(ak_meshoptprobe PROPERTIES FOLDER "Tools")
