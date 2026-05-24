add_library(ak_assets STATIC
    src/AssetDatabase.cpp
    src/AssetRegistry.cpp
)

add_library(AK::Assets ALIAS ak_assets)

target_include_directories(ak_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_assets
    PUBLIC
        AK::Core
        AK::Filesystem
)

set_target_properties(ak_assets PROPERTIES FOLDER "Engine")
