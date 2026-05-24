add_executable(ak_explosionprobe
    src/main.cpp
)

target_link_libraries(ak_explosionprobe
    PRIVATE
        AK::Core
        AK::Explosion
)

set_target_properties(ak_explosionprobe PROPERTIES FOLDER "Tools")
