add_library(ak_outofcore STATIC
    src/OutOfCore.cpp
)

add_library(AK::OutOfCore ALIAS ak_outofcore)

target_include_directories(ak_outofcore
    PUBLIC
        include
)

target_link_libraries(ak_outofcore
    PUBLIC
        AK::Core
)

set_target_properties(ak_outofcore PROPERTIES FOLDER "Engine")
