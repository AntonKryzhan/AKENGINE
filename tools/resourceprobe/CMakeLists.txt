add_executable(ak_resourceprobe
    src/main.cpp
)

target_link_libraries(ak_resourceprobe
    PRIVATE
        AK::Core
        AK::Resources
)

set_target_properties(ak_resourceprobe PROPERTIES FOLDER "Tools")
