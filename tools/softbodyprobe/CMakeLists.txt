add_executable(ak_softbodyprobe
    src/main.cpp
)

target_link_libraries(ak_softbodyprobe
    PRIVATE
        AK::Core
        AK::SoftBody
)

set_target_properties(ak_softbodyprobe PROPERTIES FOLDER "Tools")
