add_executable(ak_projectileprobe
    src/main.cpp
)

target_link_libraries(ak_projectileprobe
    PRIVATE
        AK::Core
        AK::Projectile
)

set_target_properties(ak_projectileprobe PROPERTIES FOLDER "Tools")
