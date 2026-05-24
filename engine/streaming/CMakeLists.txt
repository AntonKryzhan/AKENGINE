add_library(ak_streaming
    include/AK/Streaming/StreamingSystem.hpp
    src/StreamingSystem.cpp
)

add_library(AK::Streaming ALIAS ak_streaming)

target_include_directories(ak_streaming
    PUBLIC
        include
)

target_link_libraries(ak_streaming
    PUBLIC
        AK::Core
        AK::Resources
        AK::VFS
    PRIVATE
        AK::Assets
        AK::Filesystem
        AK::Package
)

set_target_properties(ak_streaming PROPERTIES FOLDER "Engine")
