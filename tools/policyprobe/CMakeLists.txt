add_executable(ak_policyprobe
    src/main.cpp
)

target_link_libraries(ak_policyprobe
    PRIVATE
        AK::Core
        AK::Policy
)

set_target_properties(ak_policyprobe PROPERTIES FOLDER "Tools")
