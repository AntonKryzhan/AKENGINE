add_executable(ak_geomprobe
    src/main.cpp
)

target_link_libraries(ak_geomprobe
    PRIVATE
        AK::Core
        AK::Math
)

set_target_properties(ak_geomprobe PROPERTIES FOLDER "Tools")
