add_library(ak_filesystem
    include/AK/Filesystem/FileSystem.hpp
    src/FileSystem.cpp
)

add_library(AK::Filesystem ALIAS ak_filesystem)

target_include_directories(ak_filesystem
    PUBLIC
        include
)

target_link_libraries(ak_filesystem
    PUBLIC
        AK::Core
)

set_target_properties(ak_filesystem PROPERTIES FOLDER "Engine")
