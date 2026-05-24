add_library(ak_geodata STATIC
    src/GeoData.cpp
)

add_library(AK::GeoData ALIAS ak_geodata)

target_include_directories(ak_geodata
    PUBLIC
        include
)

target_link_libraries(ak_geodata
    PUBLIC
        AK::Core
        AK::Math
        AK::WorldTopology
)

set_target_properties(ak_geodata PROPERTIES FOLDER "Engine")
