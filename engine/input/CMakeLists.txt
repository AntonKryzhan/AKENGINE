add_library(ak_input STATIC
    src/InputActions.cpp
)

add_library(AK::Input ALIAS ak_input)

target_include_directories(ak_input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_input
    PUBLIC
        AK::Core
        AK::Platform
)

set_target_properties(ak_input PROPERTIES FOLDER "Engine")
