add_executable(ak_vfsprobe
    src/main.cpp
)

target_link_libraries(ak_vfsprobe
    PRIVATE
        AK::Core
        AK::VFS
)

set_target_properties(ak_vfsprobe PROPERTIES FOLDER "Tools")
