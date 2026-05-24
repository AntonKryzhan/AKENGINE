add_library(ak_resources
    include/AK/Resources/ResourceManager.hpp
    src/ResourceManager.cpp
)

add_library(AK::Resources ALIAS ak_resources)

target_include_directories(ak_resources
    PUBLIC
        include
)

target_link_libraries(ak_resources
    PUBLIC
        AK::Core
)

set_target_properties(ak_resources PROPERTIES FOLDER "Engine")
