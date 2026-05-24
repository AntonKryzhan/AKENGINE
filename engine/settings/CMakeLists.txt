add_library(ak_settings
    include/AK/Settings/ProjectSettings.hpp
    src/ProjectSettings.cpp
)

add_library(AK::Settings ALIAS ak_settings)

target_include_directories(ak_settings
    PUBLIC
        include
)

target_link_libraries(ak_settings
    PUBLIC
        AK::Core
    PRIVATE
        AK::Filesystem
)

set_target_properties(ak_settings PROPERTIES FOLDER "Engine")
