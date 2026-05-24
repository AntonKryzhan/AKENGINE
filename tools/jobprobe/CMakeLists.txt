add_executable(ak_jobprobe
    src/main.cpp
)

target_link_libraries(ak_jobprobe
    PRIVATE
        AK::Core
        AK::Jobs
)

set_target_properties(ak_jobprobe PROPERTIES FOLDER "Tools")
