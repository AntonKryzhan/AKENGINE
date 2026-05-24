add_executable(ak_forcefieldprobe
    src/main.cpp
)

target_link_libraries(ak_forcefieldprobe
    PRIVATE
        AK::ForceField
)

set_target_properties(ak_forcefieldprobe PROPERTIES FOLDER "Tools")
