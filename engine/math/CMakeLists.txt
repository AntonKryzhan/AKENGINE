add_library(ak_math STATIC
    src/Chernoff.cpp
    src/Geometry.cpp
    src/Numerics.cpp
    src/Transform.cpp
)

target_include_directories(ak_math
    PUBLIC
        include
)

target_link_libraries(ak_math
    PUBLIC
        AK::Core
)

add_library(AK::Math ALIAS ak_math)

set_target_properties(ak_math PROPERTIES FOLDER "Engine")
