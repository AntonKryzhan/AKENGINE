add_library(ak_diagnostics
    include/AK/Diagnostics/Diagnostics.hpp
    src/Diagnostics.cpp
)

add_library(AK::Diagnostics ALIAS ak_diagnostics)

target_include_directories(ak_diagnostics
    PUBLIC
        include
)

target_link_libraries(ak_diagnostics
    PUBLIC
        AK::Core
)

set_target_properties(ak_diagnostics PROPERTIES FOLDER "Engine")
