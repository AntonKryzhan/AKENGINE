add_executable(ak_ragdollprobe
    src/main.cpp
)

target_link_libraries(ak_ragdollprobe
    PRIVATE
        AK::Core
        AK::Ragdoll
)

set_target_properties(ak_ragdollprobe PROPERTIES FOLDER "Tools")
