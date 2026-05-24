add_executable(ak_streamingprobe
    src/main.cpp
)

target_link_libraries(ak_streamingprobe
    PRIVATE
        AK::Core
        AK::Streaming
)

set_target_properties(ak_streamingprobe PROPERTIES FOLDER "Tools")
