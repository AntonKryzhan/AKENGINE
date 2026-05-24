add_library(ak_serialization
    include/AK/Serialization/Schema.hpp
    src/Schema.cpp
)

add_library(AK::Serialization ALIAS ak_serialization)

target_include_directories(ak_serialization
    PUBLIC
        include
)

target_link_libraries(ak_serialization
    PUBLIC
        AK::Core
)

set_target_properties(ak_serialization PROPERTIES FOLDER "Engine")
