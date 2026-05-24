add_executable(ak_destructionprobe
    src/main.cpp
)

target_link_libraries(ak_destructionprobe
    PRIVATE
        AK::Core
        AK::Destruction
)

set_target_properties(ak_destructionprobe PROPERTIES FOLDER "Tools")
