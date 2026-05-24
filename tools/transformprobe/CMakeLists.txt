add_executable(ak_transformprobe
    src/main.cpp
)

target_link_libraries(ak_transformprobe
    PRIVATE
        AK::Core
        AK::Math
)

set_target_properties(ak_transformprobe PROPERTIES FOLDER "Tools")
