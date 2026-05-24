add_library(ak_ntc STATIC
    src/NeuralTextureCompression.cpp
)

add_library(AK::NTC ALIAS ak_ntc)

target_include_directories(ak_ntc
    PUBLIC
        include
)

target_link_libraries(ak_ntc
    PUBLIC
        AK::Core
        AK::Texture
)
