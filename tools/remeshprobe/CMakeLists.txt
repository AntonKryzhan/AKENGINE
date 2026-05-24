add_executable(ak_remeshprobe
    src/main.cpp
)

target_link_libraries(ak_remeshprobe
    PRIVATE
        AK::Core
        AK::Remesh
)

set_target_properties(ak_remeshprobe PROPERTIES FOLDER "Tools")
