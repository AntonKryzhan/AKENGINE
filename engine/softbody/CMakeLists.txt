add_library(ak_softbody STATIC
    src/SoftBody.cpp
)

add_library(AK::SoftBody ALIAS ak_softbody)

target_include_directories(ak_softbody
    PUBLIC
        include
)

target_link_libraries(ak_softbody
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_softbody PROPERTIES FOLDER "Engine")
