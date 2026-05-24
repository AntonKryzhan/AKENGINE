add_executable(ak_assetc
    src/main.cpp
)

target_link_libraries(ak_assetc
    PRIVATE
        AK::Core
        AK::Assets
        AK::Package
)

set_target_properties(ak_assetc PROPERTIES FOLDER "Tools")
