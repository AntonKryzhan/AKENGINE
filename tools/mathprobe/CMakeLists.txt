add_executable(ak_mathprobe
    src/main.cpp
)

target_link_libraries(ak_mathprobe
    PRIVATE
        AK::Core
        AK::Math
)

set_target_properties(ak_mathprobe PROPERTIES FOLDER "Tools")
