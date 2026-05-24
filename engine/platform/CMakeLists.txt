add_library(ak_platform STATIC
    src/Window.cpp
)

add_library(AK::Platform ALIAS ak_platform)

target_include_directories(ak_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_platform
    PUBLIC
        AK::Core
)

if(WIN32)
    target_link_libraries(ak_platform PRIVATE user32)
endif()

set_target_properties(ak_platform PROPERTIES FOLDER "Engine")
