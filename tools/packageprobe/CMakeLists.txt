add_executable(ak_packageprobe
    src/main.cpp
)

target_link_libraries(ak_packageprobe
    PRIVATE
        AK::Core
        AK::Package
)

set_target_properties(ak_packageprobe PROPERTIES FOLDER "Tools")
