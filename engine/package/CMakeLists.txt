add_library(ak_package STATIC
    src/AssetPackage.cpp
)

add_library(AK::Package ALIAS ak_package)

target_include_directories(ak_package
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_package
    PUBLIC
        AK::Core
        AK::Filesystem
        AK::Assets
)

set_target_properties(ak_package PROPERTIES FOLDER "Engine")
