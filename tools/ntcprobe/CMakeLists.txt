add_executable(ak_ntcprobe
    src/main.cpp
)

target_link_libraries(ak_ntcprobe
    PRIVATE
        AK::Core
        AK::Texture
        AK::NTC
)

set_target_properties(ak_ntcprobe PROPERTIES FOLDER "Tools")
