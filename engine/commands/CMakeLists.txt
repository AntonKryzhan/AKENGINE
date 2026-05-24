add_library(ak_commands
    include/AK/Commands/CommandRegistry.hpp
    src/CommandRegistry.cpp
)

add_library(AK::Commands ALIAS ak_commands)

target_include_directories(ak_commands
    PUBLIC
        include
)

target_link_libraries(ak_commands
    PUBLIC
        AK::Input
)

set_target_properties(ak_commands PROPERTIES FOLDER "Engine")
