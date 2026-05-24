add_executable(ak_characterprobe
    src/main.cpp
)

target_link_libraries(ak_characterprobe
    PRIVATE
        AK::Core
        AK::Character
)

set_target_properties(ak_characterprobe PROPERTIES FOLDER "Tools")
