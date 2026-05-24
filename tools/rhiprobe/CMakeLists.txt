add_executable(ak_rhiprobe
    src/main.cpp
)

target_link_libraries(ak_rhiprobe
    PRIVATE
        AK::Core
        AK::RHI
        AK::Render
)

set_target_properties(ak_rhiprobe PROPERTIES FOLDER "Tools")
