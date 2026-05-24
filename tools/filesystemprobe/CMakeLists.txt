add_executable(ak_filesystemprobe
    src/main.cpp
)

target_link_libraries(ak_filesystemprobe
    PRIVATE
        AK::Core
        AK::Filesystem
)

set_target_properties(ak_filesystemprobe PROPERTIES FOLDER "Tools")
