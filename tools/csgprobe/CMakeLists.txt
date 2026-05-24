add_executable(ak_csgprobe
    src/main.cpp
)

target_link_libraries(ak_csgprobe
    PRIVATE
        AK::Core
        AK::CSG
)

set_target_properties(ak_csgprobe PROPERTIES FOLDER "Tools")
