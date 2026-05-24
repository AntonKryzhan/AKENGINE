add_library(ak_memory
    include/AK/Memory/Memory.hpp
    src/Memory.cpp
)

add_library(AK::Memory ALIAS ak_memory)

target_include_directories(ak_memory
    PUBLIC
        include
)

target_link_libraries(ak_memory
    PUBLIC
        AK::Core
)

set_target_properties(ak_memory PROPERTIES FOLDER "Engine")
