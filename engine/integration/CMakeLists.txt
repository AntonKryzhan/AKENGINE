add_library(ak_integration STATIC
    src/EngineIntegration.cpp
)

add_library(AK::Integration ALIAS ak_integration)

target_include_directories(ak_integration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_integration
    PUBLIC
        AK::Core
)

set_target_properties(ak_integration PROPERTIES FOLDER "Engine")
