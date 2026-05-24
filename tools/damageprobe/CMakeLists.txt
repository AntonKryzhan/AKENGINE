add_executable(ak_damageprobe
    src/main.cpp
)

target_link_libraries(ak_damageprobe
    PRIVATE
        AK::Core
        AK::Damage
)

set_target_properties(ak_damageprobe PROPERTIES FOLDER "Tools")
