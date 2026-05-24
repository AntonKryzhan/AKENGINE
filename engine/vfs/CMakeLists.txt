add_library(ak_vfs STATIC
    src/VirtualFileSystem.cpp
)

add_library(AK::VFS ALIAS ak_vfs)

target_include_directories(ak_vfs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_vfs
    PUBLIC
        AK::Core
        AK::Filesystem
        AK::Assets
        AK::Package
)

set_target_properties(ak_vfs PROPERTIES FOLDER "Engine")
