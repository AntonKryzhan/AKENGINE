add_executable(ak_vulkandrawprobe
    src/main.cpp
)

target_link_libraries(ak_vulkandrawprobe
    PRIVATE
        AK::Core
        AK::Math
        AK::Platform
        AK::RHI
        AK::Render
)

set_target_properties(ak_vulkandrawprobe PROPERTIES FOLDER "Tools")
