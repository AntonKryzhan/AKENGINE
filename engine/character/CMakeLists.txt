add_library(ak_character STATIC
    src/CharacterController.cpp
)

add_library(AK::Character ALIAS ak_character)

target_include_directories(ak_character
    PUBLIC
        include
)

target_link_libraries(ak_character
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_character PROPERTIES FOLDER "Engine")
