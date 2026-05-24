add_library(ak_texture STATIC
    src/TextureSet.cpp
)

add_library(AK::Texture ALIAS ak_texture)

target_include_directories(ak_texture
    PUBLIC
        include
)

target_link_libraries(ak_texture
    PUBLIC
        AK::Core
)
